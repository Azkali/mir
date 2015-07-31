/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#define MIR_LOG_COMPONENT "MirBufferStream"

#include "buffer_stream.h"
#include "make_protobuf_object.h"

#include "buffer_vault.h"

#include "mir_connection.h"
#include "mir/frontend/client_constants.h"

#include "mir/log.h"

#include "mir_toolkit/mir_native_buffer.h"
#include "mir/client_platform.h"
#include "mir/egl_native_window_factory.h"

#include "perf_report.h"
#include "logging/perf_report.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include <stdexcept>

namespace mcl = mir::client;
namespace mf = mir::frontend;
namespace ml = mir::logging;
namespace mp = mir::protobuf;
namespace geom = mir::geometry;

namespace gp = google::protobuf;

namespace mir
{
namespace client
{
struct Amorphous
{
    virtual void deposit(
        protobuf::Buffer const&, geometry::Size, MirPixelFormat) = 0;
    virtual void set_buffer_cache_size(unsigned int) = 0;
    virtual std::shared_ptr<mir::client::ClientBuffer> get_current_buffer() = 0;
    virtual uint32_t get_current_buffer_id() = 0;
    virtual MirWaitHandle* submit(std::function<void()> const&, geometry::Size sz, int stream_id) = 0;
    virtual ~Amorphous() = default;
};
}
}

namespace
{

void populate_buffer_package(
    MirBufferPackage& buffer_package,
    mir::protobuf::Buffer const& protobuf_buffer)
{
    if (!protobuf_buffer.has_error())
    {
        buffer_package.data_items = protobuf_buffer.data_size();
        for (int i = 0; i != protobuf_buffer.data_size(); ++i)
        {
            buffer_package.data[i] = protobuf_buffer.data(i);
        }

        buffer_package.fd_items = protobuf_buffer.fd_size();

        for (int i = 0; i != protobuf_buffer.fd_size(); ++i)
        {
            buffer_package.fd[i] = protobuf_buffer.fd(i);
        }

        buffer_package.stride = protobuf_buffer.stride();
        buffer_package.flags = protobuf_buffer.flags();
        buffer_package.width = protobuf_buffer.width();
        buffer_package.height = protobuf_buffer.height();
    }
    else
    {
        buffer_package.data_items = 0;
        buffer_package.fd_items = 0;
        buffer_package.stride = 0;
        buffer_package.flags = 0;
        buffer_package.width = 0;
        buffer_package.height = 0;
    }
}


struct OldBufferSemantics : mcl::Amorphous
{
    OldBufferSemantics(
        mir::protobuf::DisplayServer& server,
        std::shared_ptr<mcl::ClientBufferFactory> const& factory, int max_buffers) :
        wrapped{factory, max_buffers},
        display_server(server), first{true}
    {
    }

    void deposit(mp::Buffer const& buffer, geom::Size size, MirPixelFormat pf)
    {
        if (first || (on_incoming_buffer))
        {
            first = false;
            auto buffer_package = std::make_shared<MirBufferPackage>();
            populate_buffer_package(*buffer_package, buffer);
            wrapped.deposit_package(buffer_package, buffer.buffer_id(), size, pf);
            if (on_incoming_buffer)
                on_incoming_buffer();
            on_incoming_buffer = std::function<void()>{};
            next_buffer_wait_handle.result_received();
        }
        else
        {
            incoming_buffers.push(buffer);
        }
    }

    void set_buffer_cache_size(unsigned int sz) override
    {
        wrapped.set_max_buffers(sz);
    }
    std::shared_ptr<mir::client::ClientBuffer> get_current_buffer() override
    {
        return wrapped.current_buffer();
    }
    uint32_t get_current_buffer_id() override
    {
        if (incoming_buffers.size())
            return incoming_buffers.front().buffer_id();
        return wrapped.current_buffer_id();
    }

    MirWaitHandle* submit(std::function<void()> const& done, geom::Size sz, int stream_id) override
    {
        //always submit what we have, whether we have a buffer, or will have to wait for an async reply
        auto request = mcl::make_protobuf_object<mp::BufferRequest>();
        request->mutable_id()->set_value(stream_id);
        request->mutable_buffer()->set_buffer_id(wrapped.current_buffer_id());
        //lock.unlock();

        display_server.submit_buffer(nullptr, request.get(), protobuf_void.get(),
            google::protobuf::NewCallback(google::protobuf::DoNothing));

        //lock.lock();
        if (incoming_buffers.empty())
        {
            next_buffer_wait_handle.expect_result();
            on_incoming_buffer = done; 
        }
        else
        {
            auto buffer_package = std::make_shared<MirBufferPackage>();
            populate_buffer_package(*buffer_package, incoming_buffers.front());
            wrapped.deposit_package(buffer_package, incoming_buffers.front().buffer_id(), sz, mir_pixel_format_abgr_8888);
            incoming_buffers.pop();
            done();
        }
        return &next_buffer_wait_handle;
    }

    mcl::ClientBufferDepository wrapped;
    mir::protobuf::DisplayServer& display_server;
    std::function<void()> on_incoming_buffer;
    std::queue<mir::protobuf::Buffer> incoming_buffers;
    std::unique_ptr<mir::protobuf::Void> protobuf_void{std::make_unique<mp::Void>()};
    MirWaitHandle next_buffer_wait_handle;
    bool first;
};

struct NewBufferSemantics : mcl::Amorphous
{
    NewBufferSemantics(
        std::shared_ptr<mcl::ClientBufferFactory> const& factory,
        std::shared_ptr<mcl::ServerBufferRequests> const& requests,
        geom::Size size, MirPixelFormat format, int usage,
        unsigned int initial_nbuffers) :
        vault(factory, requests, size, format, usage, initial_nbuffers)
    {
    }

    void deposit(mp::Buffer const& buffer, geom::Size, MirPixelFormat)
    {
        vault.wire_transfer_inbound(buffer);
    }
    void set_buffer_cache_size(unsigned int) override
    {
    }
    std::shared_ptr<mir::client::ClientBuffer> get_current_buffer() override
    {
        //vault.withdraw_buffer();
        throw std::logic_error("if a cat is a tabby, all tabbies are cats");
    }
    uint32_t get_current_buffer_id() override
    {
        throw std::logic_error("if a cat is a tabby, all tabbies are cats");
    }
    MirWaitHandle* submit(std::function<void()> const&, geom::Size, int) override
    {
        throw std::logic_error("if a cat is a tabby, all tabbies are cats");
    }

    mcl::BufferVault vault;
    std::shared_ptr<mcl::ClientBuffer> current_buffer;
    int current_buffer_id;
};

}

mcl::BufferStream::BufferStream(
    MirConnection* connection,
    mp::DisplayServer& server,
    mcl::BufferStreamMode mode,
    std::shared_ptr<mcl::ClientPlatform> const& client_platform,
    mp::BufferStream const& protobuf_bs,
    std::shared_ptr<mcl::PerfReport> const& perf_report,
    std::string const& surface_name)
    : connection(connection),
      display_server(server),
      mode(mode),
      client_platform(client_platform),
      protobuf_bs{mcl::make_protobuf_object<mir::protobuf::BufferStream>(protobuf_bs)},
      swap_interval_(1),
      perf_report(perf_report),
      protobuf_void{mcl::make_protobuf_object<mir::protobuf::Void>()}
{
    created(nullptr, nullptr);
    perf_report->name_surface(surface_name.c_str());
}

mcl::BufferStream::BufferStream(
    MirConnection* connection,
    mp::DisplayServer& server,
    std::shared_ptr<mcl::ClientPlatform> const& client_platform,
    mp::BufferStreamParameters const& parameters,
    std::shared_ptr<mcl::PerfReport> const& perf_report,
    mir_buffer_stream_callback callback,
    void *context)
    : connection(connection),
      display_server(server),
      mode(BufferStreamMode::Producer),
      client_platform(client_platform),
      protobuf_bs{mcl::make_protobuf_object<mir::protobuf::BufferStream>()},
      swap_interval_(1),
      perf_report(perf_report),
      protobuf_void{mcl::make_protobuf_object<mir::protobuf::Void>()}
{
    perf_report->name_surface(std::to_string(reinterpret_cast<long int>(this)).c_str());

    create_wait_handle.expect_result();
    try
    {
        server.create_buffer_stream(0, &parameters, protobuf_bs.get(), gp::NewCallback(this, &mcl::BufferStream::created, callback,
            context));
    }
    catch (std::exception const& ex)
    {
        protobuf_bs->set_error(std::string{"Error invoking create buffer stream: "} +
                              boost::diagnostic_information(ex));
    }
        
}

namespace
{
class Requests : public mcl::ServerBufferRequests
{
public:
    void allocate_buffer(geom::Size size, MirPixelFormat format, int usage) override
    {
        (void) size; (void) format; (void) usage;
    }
    void free_buffer(int buffer_id) override
    {
        (void) buffer_id;
    }
    void submit_buffer(mcl::ClientBuffer&) override
    {
    }
};
}


void mcl::BufferStream::created(mir_buffer_stream_callback callback, void *context)
{
    if (!protobuf_bs->has_id() || protobuf_bs->has_error())
        BOOST_THROW_EXCEPTION(std::runtime_error("Can not create buffer stream: " + std::string(protobuf_bs->error())));

    if (protobuf_bs->has_buffer())
    {
        buffer_depository = std::make_unique<OldBufferSemantics>(display_server,
            client_platform->create_buffer_factory(), mir::frontend::client_buffer_cache_size);
        process_buffer(protobuf_bs->buffer());
    }
    else
    {
        int initial_nbuffers = 3u;
        buffer_depository = std::make_unique<NewBufferSemantics>(
            client_platform->create_buffer_factory(),
            std::make_shared<Requests>(),
            geom::Size{0,0}, mir_pixel_format_abgr_8888, 0, initial_nbuffers);
    }


    egl_native_window_ = client_platform->create_egl_native_window(this);

    if (connection)
        connection->on_stream_created(protobuf_bs->id().value(), this);

    if (callback)
        callback(reinterpret_cast<MirBufferStream*>(this), context);
    create_wait_handle.result_received();
}

mcl::BufferStream::~BufferStream()
{
}

void mcl::BufferStream::process_buffer(mp::Buffer const& buffer)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    process_buffer(buffer, lock);
}

void mcl::BufferStream::process_buffer(protobuf::Buffer const& buffer, std::unique_lock<std::mutex> const&)
{
    if (buffer.has_width() && buffer.has_height())
    {
        cached_buffer_size = geom::Size{buffer.width(), buffer.height()};
    }
    
    if (buffer.has_error())
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("BufferStream received buffer with error:" + buffer.error()));
    }

    try
    {
        auto pixel_format = static_cast<MirPixelFormat>(protobuf_bs->pixel_format());
        buffer_depository->deposit(buffer, cached_buffer_size, pixel_format);
        perf_report->begin_frame(buffer.buffer_id());
    }
    catch (const std::runtime_error& err)
    {
        mir::log_error("Error processing incoming buffer " + std::string(err.what()));
    }
}

//MirWaitHandle* mcl::BufferStream::submit(std::function<void()> const& done, std::unique_lock<std::mutex> lock)
//{
//}

MirWaitHandle* mcl::BufferStream::next_buffer(std::function<void()> const& done)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    perf_report->end_frame(buffer_depository->get_current_buffer_id());

    secured_region.reset();

    // TODO: We can fix the strange "ID casting" used below in the second phase
    // of buffer stream which generalizes and clarifies the server side logic.
    if (mode == mcl::BufferStreamMode::Producer)
    {
        if (server_connection_lost)
            BOOST_THROW_EXCEPTION(std::runtime_error("new buffer unavailable"));
        lock.unlock();
        return buffer_depository->submit(done, cached_buffer_size, protobuf_bs->id().value());
    }
    else
    {
        auto screencast_id = mcl::make_protobuf_object<mp::ScreencastId>();
        screencast_id->set_value(protobuf_bs->id().value());

        lock.unlock();
        screencast_wait_handle.expect_result();

        display_server.screencast_buffer(
            nullptr,
            screencast_id.get(),
            protobuf_bs->mutable_buffer(),
            google::protobuf::NewCallback(
            this, &mcl::BufferStream::next_buffer_received,
            done));
    }

    return &screencast_wait_handle;
}


EGLNativeWindowType mcl::BufferStream::egl_native_window()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return *egl_native_window_;
}

void mcl::BufferStream::release_cpu_region()
{
    secured_region.reset();
}

std::shared_ptr<mcl::MemoryRegion> mcl::BufferStream::secure_for_cpu_write()
{
    std::unique_lock<decltype(mutex)> lock(mutex);

    secured_region = buffer_depository->get_current_buffer()->secure_for_cpu_write();
    return secured_region;
}

void mcl::BufferStream::next_buffer_received(std::function<void()> done)
{
    process_buffer(protobuf_bs->buffer());
    done();
    screencast_wait_handle.expect_result();
}

/* mcl::EGLNativeSurface interface for EGLNativeWindow integration */
MirSurfaceParameters mcl::BufferStream::get_parameters() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return MirSurfaceParameters{
        "",
        cached_buffer_size.width.as_int(),
        cached_buffer_size.height.as_int(),
        static_cast<MirPixelFormat>(protobuf_bs->pixel_format()),
        static_cast<MirBufferUsage>(protobuf_bs->buffer_usage()),
        mir_display_output_id_invalid};
}

void mcl::BufferStream::request_and_wait_for_next_buffer()
{
    next_buffer([](){})->wait_for_all();
    if (server_connection_lost)
        BOOST_THROW_EXCEPTION(std::runtime_error("new buffer unavailable"));
}

void mcl::BufferStream::on_configured()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    configure_wait_handle.result_received();
}

// TODO: It's a little strange that we use SurfaceAttrib here for the swap interval
// we take advantage of the confusion between buffer stream ids and surface ids...
// this should be resolved in the second phase of buffer stream which introduces the
// new server support.
void mcl::BufferStream::request_and_wait_for_configure(MirSurfaceAttrib attrib, int value)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    if (attrib != mir_surface_attrib_swapinterval)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Attempt to configure surface attribute " + std::to_string(attrib) +
        " on BufferStream but only mir_surface_attrib_swapinterval is supported")); 
    }
    if (mode != mcl::BufferStreamMode::Producer)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Attempt to set swap interval on screencast is invalid"));
    }

    auto setting = mcl::make_protobuf_object<mp::SurfaceSetting>();
    auto result = mcl::make_protobuf_object<mp::SurfaceSetting>();
    setting->mutable_surfaceid()->set_value(protobuf_bs->id().value());
    setting->set_attrib(attrib);
    setting->set_ivalue(value);
    lock.unlock();

    configure_wait_handle.expect_result();
    display_server.configure_surface(0, setting.get(), result.get(),
        google::protobuf::NewCallback(this, &mcl::BufferStream::on_configured));

    configure_wait_handle.wait_for_all();

    lock.lock();
    swap_interval_ = result->ivalue();
}

std::shared_ptr<mcl::ClientBuffer> mcl::BufferStream::get_current_buffer()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return buffer_depository->get_current_buffer();
}
uint32_t mcl::BufferStream::get_current_buffer_id()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return buffer_depository->get_current_buffer_id();
}

int mcl::BufferStream::swap_interval() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return swap_interval_;
}

void mcl::BufferStream::set_swap_interval(int interval)
{
    request_and_wait_for_configure(mir_surface_attrib_swapinterval, interval);
}

MirNativeBuffer* mcl::BufferStream::get_current_buffer_package()
{
    auto buffer = get_current_buffer();
    auto handle = buffer->native_buffer_handle();
    return client_platform->convert_native_buffer(handle.get());
}

MirPlatformType mcl::BufferStream::platform_type()
{
    return client_platform->platform_type();
}

MirWaitHandle* mcl::BufferStream::get_create_wait_handle()
{
    return &create_wait_handle;
}

MirWaitHandle* mcl::BufferStream::release(
        mir_buffer_stream_callback callback, void* context)
{
    if (connection)
        return connection->release_buffer_stream(this, callback, context);
    else 
        return nullptr;
}

mf::BufferStreamId mcl::BufferStream::rpc_id() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    
    return mf::BufferStreamId(protobuf_bs->id().value());
}

bool mcl::BufferStream::valid() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return protobuf_bs->has_id() && !protobuf_bs->has_error();
}

void mcl::BufferStream::buffer_available(mir::protobuf::Buffer const& buffer)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    process_buffer(buffer, lock);
}

void mcl::BufferStream::buffer_unavailable()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    server_connection_lost = true;

/*    if (on_incoming_buffer)
    {
        on_incoming_buffer();
        on_incoming_buffer = std::function<void()>{};
    }
    next_buffer_wait_handle.result_received(); */
}

void mcl::BufferStream::set_buffer_cache_size(unsigned int cache_size)
{
    std::unique_lock<std::mutex> lock(mutex);
    buffer_depository->set_buffer_cache_size(cache_size);
}
