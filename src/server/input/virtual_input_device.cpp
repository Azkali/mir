/*
 * Copyright © 2022 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mir/input/virtual_input_device.h"

#include "mir/input/pointer_settings.h"
#include "mir/input/touchpad_settings.h"
#include "mir/input/touchscreen_settings.h"

#include <atomic>

namespace mi = mir::input;

namespace
{
auto unique_device_id() -> std::string
{
    static std::atomic<int> next_id = 0;
    auto const id = next_id.fetch_add(1);
    return "virt-dev-" + std::to_string(id);
}
}

mi::VirtualInputDevice::VirtualInputDevice(std::string const& name, DeviceCapabilities capabilities)
    : info{name, unique_device_id(), capabilities}
{
}

void mi::VirtualInputDevice::if_started_then(std::function<void(InputSink*, EventBuilder*)> const& fn)
{
    std::lock_guard lock{mutex};
    if (sink && builder)
    {
        fn(sink, builder);
    }
}

void mi::VirtualInputDevice::start(InputSink* new_sink, EventBuilder* new_builder)
{
    std::lock_guard lock{mutex};
    sink = new_sink;
    builder = new_builder;
}

void mi::VirtualInputDevice::stop()
{
    std::lock_guard lock{mutex};
    sink = nullptr;
    builder = nullptr;
}

auto mi::VirtualInputDevice::get_pointer_settings() const -> optional_value<PointerSettings>
{
    return {};
}

auto mi::VirtualInputDevice::get_touchpad_settings() const -> optional_value<TouchpadSettings>
{
    return {};
}
auto mi::VirtualInputDevice::get_touchscreen_settings() const -> optional_value<TouchscreenSettings>
{
    return {};
}
