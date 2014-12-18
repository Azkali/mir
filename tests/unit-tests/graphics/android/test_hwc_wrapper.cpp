/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/platforms/android/real_hwc_wrapper.h"
#include "src/platforms/android/hwc_report.h"
#include "src/platforms/android/output_builder.h"
#include "mir_test_doubles/mock_hwc_composer_device_1.h"
#include "mir_test_doubles/mock_hwc_report.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;

struct HwcWrapper : public ::testing::Test
{
    HwcWrapper()
     : mock_device(std::make_shared<testing::NiceMock<mtd::MockHWCComposerDevice1>>()),
       mock_report(std::make_shared<testing::NiceMock<mtd::MockHwcReport>>()),
       virtual_display{nullptr},
       external_display{nullptr},
       primary_display{nullptr}
    {
    }

    int display_saving_fn(
        struct hwc_composer_device_1*, size_t sz, hwc_display_contents_1_t** displays)
    {
        switch (sz)
        {
            case 3:
                virtual_display = displays[2];
            case 2:
                external_display = displays[1];
            case 1:
                primary_display = displays[0];
            default:
                break;
        }
        return 0;
    }

    hwc_display_contents_1_t primary_list;
    hwc_display_contents_1_t external_list;
    hwc_display_contents_1_t virtual_list;
    std::shared_ptr<mtd::MockHWCComposerDevice1> const mock_device;
    std::shared_ptr<mtd::MockHwcReport> const mock_report;
    hwc_display_contents_1_t *virtual_display;
    hwc_display_contents_1_t *external_display;
    hwc_display_contents_1_t *primary_display;
};

TEST_F(HwcWrapper, submits_correct_prepare_parameters)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_report, report_list_submitted_to_prepare(Ref(primary_list)))
        .InSequence(seq);
    EXPECT_CALL(*mock_device, prepare_interface(mock_device.get(), 1, _))
        .InSequence(seq)
        .WillOnce(Invoke(this, &HwcWrapper::display_saving_fn));
    EXPECT_CALL(*mock_report, report_prepare_done(Ref(primary_list)))
        .InSequence(seq);

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.prepare({{&primary_list, nullptr, nullptr}});

    EXPECT_EQ(&primary_list, primary_display);
    EXPECT_EQ(nullptr, virtual_display);
    EXPECT_EQ(nullptr, external_display);
}

TEST_F(HwcWrapper, submits_correct_prepare_parameters_with_external_display)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_report, report_list_submitted_to_prepare(Ref(primary_list)))
        .InSequence(seq);
    EXPECT_CALL(*mock_device, prepare_interface(mock_device.get(), 2, _))
        .InSequence(seq)
        .WillOnce(Invoke(this, &HwcWrapper::display_saving_fn));
    EXPECT_CALL(*mock_report, report_prepare_done(Ref(primary_list)))
        .InSequence(seq);

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.prepare({{&primary_list, &external_list, nullptr}});

    EXPECT_EQ(&primary_list, primary_display);
    EXPECT_EQ(&external_list, external_display);
    EXPECT_EQ(nullptr, virtual_display);
}

TEST_F(HwcWrapper, throws_on_prepare_failure)
{
    using namespace testing;

    mga::RealHwcWrapper wrapper(mock_device, mock_report);

    EXPECT_CALL(*mock_device, prepare_interface(mock_device.get(), _, _))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_THROW({
        wrapper.prepare({{&primary_list, nullptr, nullptr}});
    }, std::runtime_error);
}

TEST_F(HwcWrapper, submits_correct_set_parameters)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_report, report_set_list(Ref(primary_list)))
        .InSequence(seq);
    EXPECT_CALL(*mock_device, set_interface(mock_device.get(), 3, _))
        .InSequence(seq)
        .WillOnce(Invoke(this, &HwcWrapper::display_saving_fn));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.set({{&primary_list, &external_list, &virtual_list}});

    EXPECT_EQ(&primary_list, primary_display);
    EXPECT_EQ(&external_list, external_display);
    EXPECT_EQ(&virtual_list, virtual_display);
}

TEST_F(HwcWrapper, throws_on_set_failure)
{
    using namespace testing;

    mga::RealHwcWrapper wrapper(mock_device, mock_report);

    EXPECT_CALL(*mock_device, set_interface(mock_device.get(), _, _))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_THROW({
        wrapper.set({{&primary_list, nullptr, nullptr}});
    }, std::runtime_error);
}

#if 0
TEST_F(HwcWrapper, register_procs_registers_and_preserves_hooks_until_destruction)
{
    using namespace testing;
    auto procs = std::make_shared<mga::HWCCallbacks>();
    EXPECT_CALL(*mock_device, registerProcs_interface(
        mock_device.get(), reinterpret_cast<hwc_procs_t*>(procs.get())))
        .Times(1);

    auto use_count = procs.use_count();
    {
        mga::RealHwcWrapper wrapper(mock_device, mock_report);
        wrapper.register_hooks(procs);
        EXPECT_THAT(procs.use_count(), Eq(use_count+1));
    }
    EXPECT_THAT(procs.use_count(), Eq(use_count));
}
#endif

TEST_F(HwcWrapper, turns_display_on)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_device, blank_interface(mock_device.get(), HWC_DISPLAY_PRIMARY, 0))
        .InSequence(seq)
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_report, report_display_on()) 
        .InSequence(seq);
    EXPECT_CALL(*mock_device, blank_interface(mock_device.get(), HWC_DISPLAY_EXTERNAL, 0))
        .InSequence(seq)
        .WillOnce(Return(-1));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.display_on(mga::DisplayName::primary);
    EXPECT_THROW({
        wrapper.display_on(mga::DisplayName::external);
    }, std::runtime_error);
}

TEST_F(HwcWrapper, turns_display_off)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_device, blank_interface(mock_device.get(), HWC_DISPLAY_PRIMARY, 1))
        .InSequence(seq)
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_report, report_display_off()) 
        .InSequence(seq);
    EXPECT_CALL(*mock_device, blank_interface(mock_device.get(), HWC_DISPLAY_EXTERNAL, 1))
        .InSequence(seq)
        .WillOnce(Return(-1));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.display_off(mga::DisplayName::primary);
    EXPECT_THROW({
        wrapper.display_off(mga::DisplayName::external);
    }, std::runtime_error);
}

TEST_F(HwcWrapper, turns_vsync_on)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_device, eventControl_interface(mock_device.get(), HWC_DISPLAY_EXTERNAL, HWC_EVENT_VSYNC, 1))
        .InSequence(seq)
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_report, report_vsync_on()) 
        .InSequence(seq);
    EXPECT_CALL(*mock_device, eventControl_interface(mock_device.get(), HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, 1))
        .InSequence(seq)
        .WillOnce(Return(-1));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.vsync_signal_on(mga::DisplayName::external);
    EXPECT_THROW({
        wrapper.vsync_signal_on(mga::DisplayName::primary);
    }, std::runtime_error);
}

TEST_F(HwcWrapper, turns_vsync_off)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_device, eventControl_interface(mock_device.get(), HWC_DISPLAY_EXTERNAL, HWC_EVENT_VSYNC, 0))
        .InSequence(seq)
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_report, report_vsync_off()) 
        .InSequence(seq);
    EXPECT_CALL(*mock_device, eventControl_interface(mock_device.get(), HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, 0))
        .InSequence(seq)
        .WillOnce(Return(-1));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    wrapper.vsync_signal_off(mga::DisplayName::external);
    EXPECT_THROW({
        wrapper.vsync_signal_off(mga::DisplayName::primary);
    }, std::runtime_error);
}

TEST_F(HwcWrapper, registers_hooks_for_driver)
{
    using namespace testing;
    mga::HWCCallbacks const* callbacks{nullptr};
    EXPECT_CALL(*mock_device, registerProcs_interface(mock_device.get(),_))
        .WillOnce(Invoke([&](struct hwc_composer_device_1*, hwc_procs_t const* procs)
            {callbacks = reinterpret_cast<mga::HWCCallbacks const*>(procs);}));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);

    ASSERT_THAT(callbacks, Ne(nullptr));
    EXPECT_THAT(callbacks->hooks.invalidate, Ne(nullptr));
    EXPECT_THAT(callbacks->hooks.vsync, Ne(nullptr));
    EXPECT_THAT(callbacks->hooks.hotplug, Ne(nullptr));
}

TEST_F(HwcWrapper, unregisters_at_destruction)
{
    using namespace testing;
    mga::HWCCallbacks const* callbacks{nullptr};
    EXPECT_CALL(*mock_device, registerProcs_interface(mock_device.get(),_))
        .WillOnce(Invoke([&](struct hwc_composer_device_1*, hwc_procs_t const* procs)
            {callbacks = reinterpret_cast<mga::HWCCallbacks const*>(procs);}));

    {
        mga::RealHwcWrapper wrapper(mock_device, mock_report);
        ASSERT_THAT(callbacks, Ne(nullptr));
        EXPECT_THAT(callbacks->self, Ne(nullptr));
    }
    EXPECT_THAT(callbacks->self, Eq(nullptr));    
}

#if 0
TEST_F(HwcWrapper, callback_calls_hwcvsync_and_can_continue_calling_after_destruction)
{
    using namespace testing;
    auto call_count = 0u;
    mga::HWCCallbacks const* callbacks{nullptr};
    EXPECT_CALL(*mock_device, registerProcs_interface(mock_device.get(),_))
        .WillOnce(Invoke([&](struct hwc_composer_device_1*, hwc_procs_t const* procs)
            {callbacks = reinterpret_cast<mga::HWCCallbacks const*>(procs);}));

    mga::RealHwcWrapper wrapper(mock_device, mock_report);
    auto subscription = wrapper.subscribe_to_events(
        [&](mga::DisplayName, std::chrono::nanoseconds){ call_count++; },
        [](mga::DisplayName, bool){},
        []{}
    );

    ASSERT_THAT(callbacks, Ne(nullptr));
    ASSERT_THAT(callbacks->hooks.vsync, Ne(nullptr));
    callbacks->hooks.vsync(&callbacks->hooks, 0, 0);

    //some bad drivers call the hooks after we close() the module.
    //After that point, we don't care, so just make sure there's something to call 
    callbacks->self.load(nullptr);
    callbacks->hooks.vsync(&callbacks->hooks, 0, 0);

    EXPECT_THAT(call_count, Eq(1));
}
#endif
