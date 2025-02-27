/*
 * Copyright © Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MIR_INPUT_INPUT_EVENT_TRANSFORMER_H_
#define MIR_INPUT_INPUT_EVENT_TRANSFORMER_H_

#include "mir/input/event_filter.h"
#include "mir/input/event_builder.h"

#include <chrono>
#include <vector>
#include <memory>

namespace mir
{
class MainLoop;
namespace input
{
class VirtualInputDevice;
class InputDeviceRegistry;
class InputSink;
class EventBuilder;
class InputEventTransformer : public EventFilter
{
public:
    class EventDispatcher
    {
    public:
        EventDispatcher(
            std::shared_ptr<MainLoop> const main_loop,
            input::InputSink* const sink,
            input::EventBuilder* const builder);

        void dispatch_key_event(
            std::optional<std::chrono::nanoseconds> timestamp,
            MirKeyboardAction action,
            xkb_keysym_t keysym,
            int scan_code);

        void dispatch_pointer_event(
            std::optional<std::chrono::nanoseconds> timestamp,
            MirPointerAction action,
            MirPointerButtons buttons,
            std::optional<mir::geometry::PointF> position,
            mir::geometry::DisplacementF motion,
            MirPointerAxisSource axis_source,
            events::ScrollAxisH h_scroll,
            events::ScrollAxisV v_scroll);

    private:
        void dispatch(mir::EventUPtr e);

        std::shared_ptr<MainLoop> const main_loop;
        input::InputSink* const sink;
        input::EventBuilder* const builder;
    };

    class Transformer
    {
    public:
        virtual ~Transformer() = default;

        // Returning true means that the event has been successfully processed and
        // shouldn't be handled by later transformers, whether the transformer is
        // accumulating events for later dispatching or has immediately dispatched
        // an event is an implementation detail of the transformer
        virtual bool transform_input_event(std::shared_ptr<EventDispatcher> const& dispatcher,  MirEvent const&) = 0;
    };

    InputEventTransformer(std::shared_ptr<InputDeviceRegistry> const&, std::shared_ptr<MainLoop> const&);
    ~InputEventTransformer();

    bool handle(MirEvent const&) override;

    void append(std::weak_ptr<Transformer> const&);

private:
    void lazily_init_virtual_input_device();

    std::mutex mutex;
    std::vector<std::weak_ptr<Transformer>> input_transformers;
    std::shared_ptr<input::VirtualInputDevice> virtual_pointer;
    std::shared_ptr<EventDispatcher> dispatcher;

    std::shared_ptr<input::InputDeviceRegistry> const input_device_registry;
    std::shared_ptr<MainLoop> const main_loop;
};
}
}

#endif
