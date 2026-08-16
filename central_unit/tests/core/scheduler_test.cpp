#include "scheduler_test.h"

#include <thread>

#include <gmock/gmock.h>

namespace SmartHome::Tests {
    void SchedulerTest::SetUp() {
        auto logger = std::make_shared<Utils::Logger>();
        auto loggerConfig = Utils::Logger::Config{};

        loggerConfig.logLevel = Utils::LogLevels::Level::None;
        loggerConfig.enableConsoleLogOutput = false;
        loggerConfig.logFile.enabled = false;
        loggerConfig.logFile.archiveOld = false;

        logger->applyConfig(loggerConfig);

        auto deviceFakeDispatch = [this](const std::string_view name, const uint id, const nlohmann::json &action) {
            mDeviceDispatches.push_back({.actionName = std::string(name), .id = id, .action = action});
        };

        mpLogger = std::make_shared<Utils::AsyncLogger>(logger, mIoContext);
        Scheduler::Config config{
            .timeProvider = mTimeProvider,
            .configCache = mConfigCache,
            .executor = mIoContext.get_executor(),
            .logger = mpLogger,
            .dispatchAction = deviceFakeDispatch
        };

        mpScheduler = Scheduler::create(std::move(config));

        mpScheduler->start();
    }


    //region Helpers
    nlohmann::json SchedulerTest::validDeviceConfigJson(uint deviceId, uint moduleId, const nlohmann::json &schedule) {
        return {
            {"id", deviceId},
            {"logic_id", 11},
            {"module_id", moduleId},
            {"name", "testModule"},
            {"type", "sensor"},
            {
                "config", {
                    {"use_cache", true},
                    {"cache_ttl", 300u},
                    {
                        "schedule", schedule
                    }
                }
            },
        };
    }

    nlohmann::json SchedulerTest::validDeviceConfigJsonAction() {
        return {
            "action", {{"method", "core.set"}, {"params", nlohmann::json::object()}}
        };
    }

    //endregion

    // TODO add tests simulating time zone change

    //region Load / parse / remove events
    TEST_F(SchedulerTest, LoadFromCache) {
        ASSERT_TRUE(mpScheduler->isRunning());
        EXPECT_EQ(mpScheduler->taskCount(), 0);

        mConfigCache.setDevice(CachedDevice(validDeviceConfigJson(1, 1)));
        mConfigCache.setDevice(CachedDevice(validDeviceConfigJson(2, 2)));
        mpScheduler->loadFromCache();

        EXPECT_EQ(mpScheduler->taskCount(), 2);
    }

    TEST_F(SchedulerTest, RemoveDevice) {
        mConfigCache.setDevice(CachedDevice(validDeviceConfigJson(1)));
        mConfigCache.setDevice(CachedDevice(validDeviceConfigJson(2)));
        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 2);

        mpScheduler->removeDevice(1);
        EXPECT_EQ(mpScheduler->taskCount(), 1);
    }

    TEST_F(SchedulerTest, GetNextRunForModule) {
        // First but disabled (1min)
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=MINUTELY;INTERVAL=1"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", false}
                                      }
                                  })));

        // // Second (5min) - should be next for module 1
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(2, 1, {
                                      {
                                          {"rrule", "FREQ=MINUTELY;INTERVAL=5"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));

        // Third (15min)
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(3, 1, {
                                      {
                                          {"rrule", "FREQ=MINUTELY;INTERVAL=15"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));

        // Should be next for module 2
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(4, 2, {
                                      {
                                          {"rrule", "FREQ=SECONDLY;INTERVAL=30"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));

        // Half past every hour - should be next for module 3
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(5, 3, {
                                      {
                                          {"rrule", "FREQ=HOURLY;INTERVAL=1"},
                                          validDeviceConfigJsonAction(),
                                          {"dtstart", "2026-01-01T00:30:00Z"},
                                          {"enabled", true}
                                      }
                                  })));

        mpScheduler->loadFromCache();
        ASSERT_EQ(mpScheduler->taskCount(), 4);

        EXPECT_THAT(mpScheduler->getNextRunForModule(1),
                    ::testing::Optional(::testing::Eq(mStartTimePoint + 5min)));

        EXPECT_THAT(mpScheduler->getNextRunForModule(2),
                    ::testing::Optional(::testing::Eq(mStartTimePoint + 30s)));

        constexpr auto nextHalfPast = std::chrono::floor<std::chrono::hours>(mStartTimePoint - 30min) + 1h + 30min;

        EXPECT_THAT(mpScheduler->getNextRunForModule(3),
                    ::testing::Optional(::testing::Eq(nextHalfPast)));
    }

    TEST_F(SchedulerTest, TaskWithoutFreqInRrule) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "BYMONTHDAY=15;COUNT=2"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));
        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, MissingOrInvalidSchedule) {
        auto deviceConfig = validDeviceConfigJson();
        deviceConfig["config"].erase("schedule");
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);

        deviceConfig["config"]["schedule"] = "invalid_schedule";
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, InvalidEntry) {
        auto deviceConfig = validDeviceConfigJson();
        deviceConfig["config"]["schedule"][0] = "invalid_entry";
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, MissingOrInvalidOrFalseEnabled) {
        auto deviceConfig = validDeviceConfigJson();
        deviceConfig["config"]["schedule"][0].erase("enabled");
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);

        deviceConfig["config"]["schedule"][0]["enabled"] = "true";
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);

        deviceConfig["config"]["schedule"][0]["enabled"] = false;
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, MissingOrInvalidRrule) {
        auto deviceConfig = validDeviceConfigJson();
        deviceConfig["config"]["schedule"][0].erase("rrule");
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);

        deviceConfig["config"]["schedule"][0]["rrule"] = {{"FREQ=DAILY"}};
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, MissingOrInvalidOrAction) {
        auto deviceConfig = validDeviceConfigJson();
        deviceConfig["config"]["schedule"][0].erase("action");
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);

        deviceConfig["config"]["schedule"][0]["action"] = "invalid_action";
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, MissingOrInvalidDtstart) {
        auto deviceConfig = validDeviceConfigJson();
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 1);

        deviceConfig["config"]["schedule"][0]["dtstart"] = 12345;
        mConfigCache.setDevice(CachedDevice(deviceConfig));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 1);
    }

    TEST_F(SchedulerTest, HandlesAncientDtstartWithoutStalling) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=SECONDLY;INTERVAL=1"},
                                          validDeviceConfigJsonAction(),
                                          {"dtstart", "2025-01-01T00:00:00Z"},
                                          {"enabled", true}
                                      }
                                  })));
        auto promiseFinished = std::make_shared<std::promise<bool> >();
        auto futureResult = promiseFinished->get_future();

        std::jthread([scheduler = mpScheduler, promiseFinished] {
            scheduler->loadFromCache();
            promiseFinished->set_value(true);
        }).detach();

        EXPECT_TRUE(futureResult.wait_for(150ms)!=std::future_status::timeout);
    }

    TEST_F(SchedulerTest, DiscardsEventThatWouldStallDueToAncientDtstart) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=SECONDLY;INTERVAL=1;COUNT=200000"},
                                          {"dtstart", "2025-01-01T00:00:00Z"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));
        auto promiseFinished = std::make_shared<std::promise<size_t> >();
        auto futureResult = promiseFinished->get_future();

        std::jthread([scheduler = mpScheduler, promiseFinished] {
            scheduler->loadFromCache();
            promiseFinished->set_value(scheduler->taskCount());
        }).detach();

        ASSERT_TRUE(futureResult.wait_for(150ms)!=std::future_status::timeout) << "Scheduler stalled";
        EXPECT_EQ(futureResult.get(), 0);
    }

    TEST_F(SchedulerTest, DiscardsEventWithNoFutureOccurence) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=DAILY;UNTIL=20251231"},
                                          {"dtstart", "2025-01-01T00:00:00Z"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));
        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    TEST_F(SchedulerTest, DiscardsExhaustedRecurrenceDuringManualAdvance) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=DAILY;INTERVAL=1;COUNT=5"},
                                          {"dtstart", "2025-01-01T00:00:00Z"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));

        mpScheduler->loadFromCache();
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    //endregion

    //region Handle / dispatch tasks
    TEST_F(SchedulerTest, DispatchTask) {
        mConfigCache.setDevice(CachedDevice(validDeviceConfigJson()));
        mpScheduler->loadFromCache();

        ASSERT_EQ(mpScheduler->taskCount(), 1);
        EXPECT_THAT(mDeviceDispatches, ::testing::IsEmpty());
        EXPECT_THAT(mpScheduler->getNextRunForModule(1),
                    ::testing::Optional(::testing::Eq(mStartTimePoint + 5min)));

        mTimeProvider.advanceBy(4min);
        EXPECT_EQ(mDeviceDispatches.size(), 0);
        mIoContext.poll();
        mTimeProvider.advanceBy(1min);
        EXPECT_EQ(mDeviceDispatches.size(), 0);
        mIoContext.restart();
        mIoContext.poll();
        EXPECT_EQ(mDeviceDispatches.size(), 1);
    }

    TEST_F(SchedulerTest, TaskWithCountLimit) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {
                                              "rrule",
                                              "FREQ=MONTHLY;BYMONTHDAY=23;COUNT=2"
                                          },
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));
        mpScheduler->loadFromCache();
        ASSERT_EQ(mpScheduler->taskCount(), 1);

        mTimeProvider.advanceBy(std::chrono::months(2));
        mIoContext.restart();
        mIoContext.poll();
        EXPECT_EQ(mDeviceDispatches.size(), 2);
    }

    TEST_F(SchedulerTest, TaskWithUntilLimit) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {
                                              "rrule",
                                              "FREQ=MONTHLY;BYMONTHDAY=23;UNTIL=20260623"
                                          },
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));
        mpScheduler->loadFromCache();
        ASSERT_EQ(mpScheduler->taskCount(), 1);

        mTimeProvider.advanceBy(std::chrono::months(6));
        mIoContext.restart();
        mIoContext.poll();
        EXPECT_EQ(mDeviceDispatches.size(), 2);
    }

    TEST_F(SchedulerTest, TaskWithoutDtstartSkippedFirstOccurrence) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=MONTHLY;COUNT=2"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));
        mpScheduler->loadFromCache();
        ASSERT_EQ(mpScheduler->taskCount(), 1);

        mTimeProvider.advanceBy(std::chrono::months(2));
        mIoContext.restart();
        mIoContext.poll();
        EXPECT_EQ(mDeviceDispatches.size(), 1);
    }

    TEST_F(SchedulerTest, SkipsRemovedTaskEncounteredDuringTimerFire) {
        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(1, 1, {
                                      {
                                          {"rrule", "FREQ=MINUTELY;INTERVAL=1;COUNT=2"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));

        mConfigCache.setDevice(CachedDevice(
            validDeviceConfigJson(2, 2, {
                                      {
                                          {"rrule", "FREQ=MINUTELY;INTERVAL=3;COUNT=2"},
                                          validDeviceConfigJsonAction(),
                                          {"enabled", true}
                                      }
                                  })));

        mpScheduler->loadFromCache();
        ASSERT_EQ(mpScheduler->taskCount(), 2);

        mpScheduler->removeDevice(2);
        ASSERT_EQ(mpScheduler->taskCount(), 1);

        mTimeProvider.advanceBy(3min);
        mIoContext.restart();
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 1);
        EXPECT_EQ(mDeviceDispatches[0].id, 1);
        EXPECT_EQ(mpScheduler->taskCount(), 0);
    }

    //endregion
}
