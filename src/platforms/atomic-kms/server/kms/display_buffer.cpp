/*
 * Copyright © Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "display_sink.h"
#include "kms_cpu_addressable_display_provider.h"
#include "kms_output.h"
#include "cpu_addressable_fb.h"
#include "gbm_display_allocator.h"
#include "mir/fd.h"
#include "mir/graphics/display_report.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/transformation.h"
#include "bypass.h"
#include "mir/fatal.h"
#include "mir/log.h"
#include "display_helpers.h"
#include "egl_helper.h"
#include "mir/graphics/egl_error.h"
#include "mir/graphics/gl_config.h"
#include "mir/graphics/dmabuf_buffer.h"

#include <boost/throw_exception.hpp>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>

#include <stdexcept>
#include <chrono>

namespace mg = mir::graphics;
namespace mga = mir::graphics::atomic;
namespace geom = mir::geometry;
namespace mgk = mir::graphics::kms;

mga::DisplaySink::DisplaySink(
    mir::Fd drm_fd,
    std::shared_ptr<struct gbm_device> gbm,
    mga::BypassOption,
    std::shared_ptr<DisplayReport> const& listener,
    std::shared_ptr<KMSOutput> output,
    geom::Rectangle const& area,
    glm::mat2 const& transformation)
    : gbm{std::move(gbm)},
      listener(listener),
      output{std::move(output)},
      area(area),
      transform{transformation},
      needs_set_crtc{false}
{
    listener->report_successful_setup_of_native_resources();

    if (this->output->has_crtc_mismatch())
    {
        mir::log_info("Clearing screen due to differing encountered and target modes");
        // TODO: Pull a supported format out of KMS rather than assuming XRGB8888
        auto initial_fb = std::make_shared<graphics::CPUAddressableFB>(
            std::move(drm_fd),
            false,
            DRMFormat{DRM_FORMAT_XRGB8888},
            area.size);

        auto mapping = initial_fb->map_writeable();
        ::memset(mapping->data(), 24, mapping->len());

        visible_fb = std::move(initial_fb);
        this->output->set_crtc(*visible_fb);
        listener->report_successful_drm_mode_set_crtc_on_construction();
    }
    listener->report_successful_display_construction();
}

mga::DisplaySink::~DisplaySink() = default;

geom::Rectangle mga::DisplaySink::view_area() const
{
    return area;
}

glm::mat2 mga::DisplaySink::transformation() const
{
    return transform;
}

void mga::DisplaySink::set_transformation(glm::mat2 const& t, geometry::Rectangle const& a)
{
    transform = t;
    area = a;
}

bool mga::DisplaySink::overlay(std::vector<DisplayElement> const& renderable_list)
{
    // TODO: implement more than the most basic case.
    if (renderable_list.size() != 1)
    {
        return false;
    }

    if (renderable_list[0].screen_positon != view_area())
    {
        return false;
    }

    if (renderable_list[0].source_position.top_left != geom::PointF {0,0} ||
        renderable_list[0].source_position.size.width.as_value() != view_area().size.width.as_int() ||
        renderable_list[0].source_position.size.height.as_value() != view_area().size.height.as_int())
    {
        return false;
    }

    if (auto fb = std::dynamic_pointer_cast<graphics::FBHandle>(renderable_list[0].buffer))
    {
        next_swap = std::move(fb);
        return true;
    }
    return false;
}

void mga::DisplaySink::for_each_display_sink(std::function<void(graphics::DisplaySink&)> const& f)
{
    f(*this);
}

void mga::DisplaySink::set_crtc(FBHandle const& forced_frame)
{
    /*
     * Note that failure to set the CRTC is not a fatal error. This can
     * happen under normal conditions when resizing VirtualBox (which
     * actually removes and replaces the virtual output each time so
     * sometimes it's really not there). Xorg often reports similar
     * errors, and it's not fatal.
     */
    if (!output->set_crtc(forced_frame))
        mir::log_error("Failed to set DRM CRTC. "
            "Screen contents may be incomplete. "
            "Try plugging the monitor in again.");
}

void mga::DisplaySink::post()
{
    if (!next_swap)
    {
        // Hey! No one has given us a next frame yet, so we don't have to change what's onscreen.
        // Sweet! We can just bail.
        return;
    }
    /*
     * Otherwise, pull the next frame into the pending slot
     */
    scheduled_fb = std::move(next_swap);
    next_swap = nullptr;

    /*
     * Try to schedule a page flip as first preference to avoid tearing.
     * We wait synchronously for this to complete.
     */
    if (!needs_set_crtc && !output->page_flip(*scheduled_fb))
        needs_set_crtc = true;

    /*
     * Fallback blitting: Not pretty, since it may tear. VirtualBox seems
     * to need to do this on every frame. [will complete in this thread]
     */
    if (needs_set_crtc)
    {
        set_crtc(*scheduled_fb);
        // SetCrtc is immediate, so the FB is now visible and we have nothing pending

        needs_set_crtc = false;
    }

    visible_fb = std::move(scheduled_fb);
    scheduled_fb = nullptr;

    using namespace std::chrono_literals;  // For operator""ms()

    // Predicted worst case render time for the next frame...
    auto predicted_render_time = 50ms;

    /*
     * TODO: Make a sensible predicited_render_time
     */

    recommend_sleep = 0ms;
    auto const min_frame_interval = 1000ms / output->max_refresh_rate();
    if (predicted_render_time < min_frame_interval)
        recommend_sleep = min_frame_interval - predicted_render_time;
}

std::chrono::milliseconds mga::DisplaySink::recommended_sleep() const
{
    return recommend_sleep;
}

void mga::DisplaySink::schedule_set_crtc()
{
    needs_set_crtc = true;
}

auto mga::DisplaySink::drm_fd() const -> mir::Fd
{
    return mir::Fd{mir::IntOwnedFd{output->drm_fd()}};
}

auto mga::DisplaySink::gbm_device() const -> std::shared_ptr<struct gbm_device>
{
    return gbm;
}

void mga::DisplaySink::set_next_image(std::unique_ptr<Framebuffer> content)
{
    std::vector<DisplayElement> const single_buffer = {
        DisplayElement {
            view_area(),
            geom::RectangleF{
                {0, 0},
                {view_area().size.width.as_value(), view_area().size.height.as_value()}},
            std::move(content)
        }
    };
    if (!overlay(single_buffer))
    {
        // Oh, oh! We should be *guaranteed* to “overlay” a single Framebuffer; this is likely a programming error
        BOOST_THROW_EXCEPTION((std::runtime_error{"Failed to post buffer to display"}));
    }
}

auto mga::DisplaySink::maybe_create_allocator(DisplayAllocator::Tag const& type_tag)
    -> DisplayAllocator*
{
    if (dynamic_cast<mg::CPUAddressableDisplayAllocator::Tag const*>(&type_tag))
    {
        if (!kms_allocator)
        {
            kms_allocator = kms::CPUAddressableDisplayAllocator::create_if_supported(drm_fd(), output->size());
        }
        return kms_allocator.get();
    }
    if (dynamic_cast<mg::GBMDisplayAllocator::Tag const*>(&type_tag))
    {
        if (!gbm_allocator)
        {
            gbm_allocator = std::make_unique<GBMDisplayAllocator>(drm_fd(), gbm, output->size());
        }
        return gbm_allocator.get();
    }
    return nullptr;
}
