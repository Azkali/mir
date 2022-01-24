/*
 * Copyright © 2021 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: William Wold <william.wold@canonical.com>
 */

#ifndef MIR_SHELL_BASIC_IDLE_HANDLER_H_
#define MIR_SHELL_BASIC_IDLE_HANDLER_H_

#include "mir/shell/idle_handler.h"

#include <memory>
#include <vector>
#include <chrono>

namespace mir
{
namespace graphics
{
class GraphicBufferAllocator;
}
namespace input
{
class Scene;
}
namespace scene
{
class IdleHub;
class IdleStateObserver;
}
namespace shell
{
class DisplayConfigurationController;

class BasicIdleHandler : public IdleHandler
{
public:
    static constexpr std::chrono::milliseconds display_off_timeout{300 * 1000};

    BasicIdleHandler(
        std::shared_ptr<scene::IdleHub> const& idle_hub,
        std::shared_ptr<input::Scene> const& input_scene,
        std::shared_ptr<graphics::GraphicBufferAllocator> const& allocator,
        std::shared_ptr<shell::DisplayConfigurationController> const& display_config_controller);

    ~BasicIdleHandler();

private:
    struct Timeout
    {
        std::chrono::milliseconds time;
        std::shared_ptr<scene::IdleStateObserver> observer;
    };

    std::shared_ptr<scene::IdleHub> const idle_hub;
    std::vector<Timeout> const timeouts;
};

}
}

#endif // MIR_SHELL_BASIC_IDLE_HANDLER_H_
