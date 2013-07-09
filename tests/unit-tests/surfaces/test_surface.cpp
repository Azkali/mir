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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/surfaces/surface.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/input/input_channel.h"

#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_surface_info.h"
#include "mir_test_doubles/mock_input_info.h"
#include "mir_test_doubles/mock_graphics_info.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test/fake_shared.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mc = mir::compositor;
namespace mi = mir::input;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{
struct MockInputChannel : public mi::InputChannel
{
    MOCK_CONST_METHOD0(server_fd, int());
    MOCK_CONST_METHOD0(client_fd, int());
};
}

TEST(SurfaceCreationParametersTest, default_creation_parameters)
{
    using namespace geom;
    msh::SurfaceCreationParameters params;
    
    geom::Point const default_point{geom::X{0}, geom::Y{0}};

    EXPECT_EQ(std::string(), params.name);
    EXPECT_EQ(Width(0), params.size.width);
    EXPECT_EQ(Height(0), params.size.height);
    EXPECT_EQ(default_point, params.top_left);
    EXPECT_EQ(mc::BufferUsage::undefined, params.buffer_usage);
    EXPECT_EQ(geom::PixelFormat::invalid, params.pixel_format);

    EXPECT_EQ(msh::a_surface(), params);
}

TEST(SurfaceCreationParametersTest, builder_mutators)
{
    using namespace geom;
    Size const size{Width{1024}, Height{768}};
    mc::BufferUsage const usage{mc::BufferUsage::hardware};
    geom::PixelFormat const format{geom::PixelFormat::abgr_8888};
    std::string name{"surface"};

    auto params = msh::a_surface().of_name(name)
                                 .of_size(size)
                                 .of_buffer_usage(usage)
                                 .of_pixel_format(format);

    EXPECT_EQ(name, params.name);
    EXPECT_EQ(size, params.size);
    EXPECT_EQ(usage, params.buffer_usage);
    EXPECT_EQ(format, params.pixel_format);
}

TEST(SurfaceCreationParametersTest, equality)
{
    using namespace geom;
    Size const size{Width{1024}, Height{768}};
    mc::BufferUsage const usage{mc::BufferUsage::hardware};
    geom::PixelFormat const format{geom::PixelFormat::abgr_8888};

    auto params0 = msh::a_surface().of_name("surface0")
                                  .of_size(size)
                                  .of_buffer_usage(usage)
                                  .of_pixel_format(format);

    auto params1 = msh::a_surface().of_name("surface1")
                                  .of_size(size)
                                  .of_buffer_usage(usage)
                                  .of_pixel_format(format);

    EXPECT_EQ(params0, params1);
    EXPECT_EQ(params1, params0);
}

TEST(SurfaceCreationParametersTest, inequality)
{
    using namespace geom;

    std::vector<Size> const sizes{{Width{1024}, Height{768}},
                                  {Width{1025}, Height{768}}};

    std::vector<mc::BufferUsage> const usages{mc::BufferUsage::hardware,
                                              mc::BufferUsage::software};

    std::vector<geom::PixelFormat> const formats{geom::PixelFormat::abgr_8888,
                                                 geom::PixelFormat::bgr_888};

    std::vector<msh::SurfaceCreationParameters> params_vec;

    for (auto const& size : sizes)
    {
        for (auto const& usage : usages)
        {
            for (auto const& format : formats)
            {
                auto cur_params = msh::a_surface().of_name("surface0")
                                                 .of_size(size)
                                                 .of_buffer_usage(usage)
                                                 .of_pixel_format(format);
                params_vec.push_back(cur_params);
                size_t cur_index = params_vec.size() - 1;

                /*
                 * Compare the current SurfaceCreationParameters with all the previously
                 * created ones.
                 */
                for (size_t i = 0; i < cur_index; i++)
                {
                    EXPECT_NE(params_vec[i], params_vec[cur_index]) << "cur_index: " << cur_index << " i: " << i;
                    EXPECT_NE(params_vec[cur_index], params_vec[i]) << "cur_index: " << cur_index << " i: " << i;
                }

            }
        }
    }
}

namespace
{

struct SurfaceCreation : public ::testing::Test
{
    virtual void SetUp()
    {
        using namespace testing;

        surface_name = "test_surfaceA";
        pf = geom::PixelFormat::abgr_8888;
        size = geom::Size{geom::Width{43}, geom::Height{420}};
        rect = geom::Rectangle{geom::Point{geom::X{0}, geom::Y{0}}, size};
        stride = geom::Stride{4 * size.width.as_uint32_t()};
        mock_buffer_stream = std::make_shared<testing::NiceMock<mtd::MockBufferStream>>();
        mock_basic_info = std::make_shared<mtd::MockSurfaceInfo>();
        mock_input_info = std::make_shared<mtd::MockInputInfo>();
        mock_graphics_info = std::make_shared<mtd::MockGraphicsInfo>();

        ON_CALL(*mock_buffer_stream, secure_client_buffer())
            .WillByDefault(Return(std::make_shared<mtd::StubBuffer>()));
        ON_CALL(*mock_basic_info, size_and_position())
            .WillByDefault(Return(rect));
    }

    std::shared_ptr<mtd::MockSurfaceInfo> mock_basic_info;
    std::shared_ptr<mtd::MockInputInfo> mock_input_info;
    std::shared_ptr<mtd::MockGraphicsInfo> mock_graphics_info;
    std::string surface_name;
    std::shared_ptr<testing::NiceMock<mtd::MockBufferStream>> mock_buffer_stream;
    geom::PixelFormat pf;
    geom::Stride stride;
    geom::Size size;
    geom::Rectangle rect;
};

}

TEST_F(SurfaceCreation, test_surface_returns_same_gfx_info)
{
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    EXPECT_EQ(mock_graphics_info, surf.graphics_info());
}

TEST_F(SurfaceCreation, test_surface_queries_stream_for_pf)
{
    using namespace testing;

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    EXPECT_CALL(*mock_buffer_stream, get_stream_pixel_format())
        .Times(1)
        .WillOnce(Return(pf));

    auto ret_pf = surf.pixel_format();

    EXPECT_EQ(ret_pf, pf);
}

TEST_F(SurfaceCreation, test_surface_gets_right_name)
{
    using namespace testing;
    EXPECT_CALL(*mock_basic_info, name())
        .Times(1)
        .WillOnce(ReturnRef(surface_name));

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    EXPECT_EQ(surface_name, surf.name());
}

TEST_F(SurfaceCreation, test_surface_queries_info_for_size)
{
    using namespace testing;
    EXPECT_CALL(*mock_basic_info, size_and_position())
        .Times(1)
        .WillOnce(Return(rect));

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    EXPECT_EQ(size, surf.size());
}

TEST_F(SurfaceCreation, test_surface_next_buffer)
{
    using namespace testing;
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    auto graphics_resource = std::make_shared<mtd::StubBuffer>();

    EXPECT_CALL(*mock_buffer_stream, secure_client_buffer())
        .Times(1)
        .WillOnce(Return(graphics_resource));

    EXPECT_EQ(graphics_resource, surf.advance_client_buffer());
}

/*
TEST_F(SurfaceCreation, test_surface_next_buffer_notifies_changes)
{
    using namespace testing;
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    auto graphics_resource = std::make_shared<mtd::StubBuffer>();

    EXPECT_CALL(*mock_buffer_stream, secure_client_buffer())
        .Times(1)
        .WillOnce(Return(graphics_resource));

    EXPECT_CALL(mock_callback, call()).Times(1);

    surf.advance_client_buffer();
}
*/

TEST_F(SurfaceCreation, test_surface_gets_ipc_from_stream)
{
    using namespace testing;

    auto stub_buffer = std::make_shared<mtd::StubBuffer>();

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    EXPECT_CALL(*mock_buffer_stream, secure_client_buffer())
        .Times(1)
        .WillOnce(Return(stub_buffer));

    auto ret_ipc = surf.advance_client_buffer();
    EXPECT_EQ(stub_buffer, ret_ipc);
}

TEST_F(SurfaceCreation, test_surface_gets_top_left)
{
    using namespace testing;

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    auto ret_top_left = surf.top_left();

    EXPECT_EQ(geom::Point(), ret_top_left);
}

TEST_F(SurfaceCreation, test_surface_move_to)
{
    using namespace testing;
    geom::Point p{geom::X{55}, geom::Y{66}};

    EXPECT_CALL(*mock_basic_info, move_to(p))
        .Times(1);

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    surf.move_to(p);
}

TEST_F(SurfaceCreation, test_surface_set_rotation)
{
    using namespace testing;

    auto angle = 60.0f;
    auto mat = glm::vec3{0.0f, 0.0f, 1.0f};
    EXPECT_CALL(*mock_basic_info, apply_rotation(FloatEq(angle), mat))
        .Times(1);

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    surf.set_rotation(angle, mat);
}

TEST_F(SurfaceCreation, test_get_input_channel)
{
    auto mock_channel = std::make_shared<MockInputChannel>();
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, mock_channel);
    EXPECT_EQ(mock_channel, surf.input_channel());
}

TEST_F(SurfaceCreation, test_surface_compositor_buffer_locks_back_buffer_from_stream)
{
    using namespace testing;

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    auto buffer_resource = std::make_shared<mtd::StubBuffer>();

    EXPECT_CALL(*mock_buffer_stream, lock_back_buffer())
        .Times(AtLeast(1))
        .WillOnce(Return(buffer_resource));

    auto comp_resource = surf.compositor_buffer();

    EXPECT_EQ(buffer_resource, comp_resource);
}


TEST_F(SurfaceCreation, test_surface_set_alpha)
{
    using namespace testing;

    float alpha = 0.5f;
    EXPECT_CALL(*mock_graphics_info, apply_alpha(FloatEq(alpha)))
        .Times(1);

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    surf.set_alpha(alpha);
}

TEST_F(SurfaceCreation, test_surface_force_requests_to_complete)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_stream, force_requests_to_complete()).Times(Exactly(1));

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    surf.force_requests_to_complete();
}

TEST_F(SurfaceCreation, test_surface_allow_framedropping)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_stream, allow_framedropping(true))
        .Times(1);

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    surf.allow_framedropping(true);
}

TEST_F(SurfaceCreation, hide_and_show)
{
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    EXPECT_CALL(*mock_graphics_info, set_hidden(true))
        .Times(1);
    surf.set_hidden(true);
    testing::Mock::VerifyAndClearExpectations(mock_graphics_info.get());
    
    EXPECT_CALL(*mock_graphics_info, set_hidden(false))
        .Times(1);
    surf.set_hidden(false);
    testing::Mock::VerifyAndClearExpectations(mock_graphics_info.get());
}

TEST_F(SurfaceCreation, test_surface_next_buffer_tells_gfx_info_on_first_frame)
{
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    EXPECT_CALL(*mock_graphics_info, frame_posted())
        .Times(0);
    surf.advance_client_buffer();
    testing::Mock::VerifyAndClearExpectations(mock_graphics_info.get());
    
    EXPECT_CALL(*mock_graphics_info, frame_posted())
        .Times(1);
    surf.advance_client_buffer();
    testing::Mock::VerifyAndClearExpectations(mock_graphics_info.get());
}

TEST_F(SurfaceCreation, input_fds)
{
    using namespace testing;
    
    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());
    EXPECT_THROW({
            surf.client_input_fd();
    }, std::logic_error);

    MockInputChannel channel;
    int const client_fd = 13;
    EXPECT_CALL(channel, client_fd()).Times(1).WillOnce(Return(client_fd));

    ms::Surface input_surf(mock_basic_info,
                           mock_graphics_info, mock_buffer_stream,
                           mock_input_info, mt::fake_shared(channel));
    EXPECT_EQ(client_fd, input_surf.client_input_fd());
}

/* todo: this test appears just for render_surfaces example apparently */
TEST_F(SurfaceCreation, flag_for_render_makes_surfaces_valid)
{
    EXPECT_CALL(*mock_graphics_info, frame_posted())
        .Times(1);

    ms::Surface surf(mock_basic_info,
                     mock_graphics_info, mock_buffer_stream,
                     mock_input_info, std::shared_ptr<mi::InputChannel>());

    surf.flag_for_render();
}
