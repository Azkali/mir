/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef MIR_LOGGING_DUMB_CONSOLE_LOGGER_H_
#define MIR_LOGGING_DUMB_CONSOLE_LOGGER_H_

#include "mir/logging/logger.h"

namespace mir
{
namespace logging
{
class DumbConsoleLogger : public Logger
{
public:
    DumbConsoleLogger();
    
protected:
    void log(Severity severity, const std::string& message, const std::string& component);
};
}
}

#endif // MIR_LOGGING_DUMB_CONSOLE_LOGGER_H_
