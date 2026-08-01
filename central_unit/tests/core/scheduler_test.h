#pragma once
#include "scheduler.h"
#include "common/time/testing/fake_time_provider.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    class SchedulerTest : public ::testing::Test {
    protected:
        struct CapturedDispatch {
            std::string actionName;
            uint id;
            nlohmann::json action;
        };

        void SetUp() override;

        static nlohmann::json validDeviceConfigJson(uint deviceId = 1,
                                                    uint moduleId = 1,
                                                    const nlohmann::json &schedule = {
                                                        {
                                                            {"rrule", "FREQ=MINUTELY;INTERVAL=5"},
                                                            {
                                                                "action",
                                                                {
                                                                    {"method", "core.set"},
                                                                    {"params", nlohmann::json::object()}
                                                                }
                                                            },
                                                            {"enabled", true}
                                                        }
                                                    });

        static nlohmann::json validDeviceConfigJsonAction();

        std::vector<CapturedDispatch> mDeviceDispatches;
        // Set time with precision to seconds (working with RRULE requires this precision)
        static constexpr auto mStartTimePoint =
                std::chrono::system_clock::time_point{
                    std::chrono::sys_days{std::chrono::April / 22 / 2026} + 12h + 34min
                };

        Time::Testing::FakeTimeProvider mTimeProvider{mStartTimePoint};
        ConfigCache mConfigCache;
        std::shared_ptr<Scheduler> mpScheduler;
        boost::asio::io_context mIoContext;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;
    };
}
