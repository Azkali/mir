/*
 * Copyright © Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MIR_TEST_DOUBLES_STUB_SESSION_LOCKER_H
#define MIR_TEST_DOUBLES_STUB_SESSION_LOCKER_H

#include "mir/frontend/session_locker.h"

namespace mir
{
namespace test
{
namespace doubles
{

namespace mf = mir::frontend;

class StubSessionLocker : public mf::SessionLocker
{
public:
    void lock() override {}
    void unlock() override {}
    void register_interest(std::weak_ptr<mf::SessionLockObserver> const&) override {}
    void register_interest(
        std::weak_ptr<mf::SessionLockObserver> const&,
        Executor&) override {}
    void unregister_interest(mf::SessionLockObserver const&) override {}
};

}
}
}

#endif //MIR_TEST_DOUBLES_STUB_SESSION_LOCKER_H
