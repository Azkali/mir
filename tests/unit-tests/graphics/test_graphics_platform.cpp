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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 *              Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/platform.h"
#include "mir/compositor/graphic_buffer_allocator.h"
#ifndef ANDROID
#include "gbm/mock_drm.h"
#include "gbm/mock_gbm.h"
#endif
#include "mir/graphics/buffer_initializer.h"
#include "mir/logging/dumb_console_logger.h"

#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace ml = mir::logging;
namespace geom = mir::geometry;

class GraphicsPlatform : public ::testing::Test
{
public:
    GraphicsPlatform() : logger(std::make_shared<ml::DumbConsoleLogger>())
    {
        using namespace testing;
        buffer_initializer = std::make_shared<mg::NullBufferInitializer>();

#ifndef ANDROID
        ON_CALL(mock_gbm, gbm_bo_get_width(_))
        .WillByDefault(Return(320));

        ON_CALL(mock_gbm, gbm_bo_get_height(_))
        .WillByDefault(Return(240));
#endif
    }
    
    std::shared_ptr<ml::Logger> logger;
    std::shared_ptr<mg::BufferInitializer> buffer_initializer;
#ifndef ANDROID
    ::testing::NiceMock<mg::gbm::MockDRM> mock_drm;
    ::testing::NiceMock<mg::gbm::MockGBM> mock_gbm;
#endif
};

TEST_F(GraphicsPlatform, buffer_allocator_creation)
{
    using namespace testing;

    EXPECT_NO_THROW (
        auto platform = mg::create_platform(logger);
        auto allocator = platform->create_buffer_allocator(buffer_initializer);

        EXPECT_TRUE(allocator.get());
    );

}

TEST_F(GraphicsPlatform, buffer_creation)
{
    auto platform = mg::create_platform(logger);
    auto allocator = platform->create_buffer_allocator(buffer_initializer);
    geom::Size size{geom::Width{320}, geom::Height{240}};
    geom::PixelFormat pf(geom::PixelFormat::rgba_8888);
    auto buffer = allocator->alloc_buffer(size, pf);

    ASSERT_TRUE(buffer.get() != NULL); 

    EXPECT_EQ(buffer->size(), size);
    EXPECT_EQ(buffer->pixel_format(), pf );
 
}

TEST_F(GraphicsPlatform, get_ipc_package)
{
    auto platform = mg::create_platform(logger);
    auto pkg = platform->get_ipc_package();

    ASSERT_TRUE(pkg.get() != NULL); 
}
