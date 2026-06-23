#include "event_handler_test.h"

namespace SmartHome {
    // Setup method for the test fixture
    void EventHandlerTest::SetUp() {
        auto logger = std::make_shared<Utils::Logger>();
        auto loggerConfig = Utils::Logger::Config{};

        loggerConfig.logLevel = Utils::LogLevels::Level::None;
        loggerConfig.enableConsoleLogOutput = false;
        loggerConfig.logFile.enabled = false;
        loggerConfig.logFile.archiveOld = false;

        logger->applyConfig(loggerConfig);

        mpLogger = std::make_shared<Utils::AsyncLogger>(logger, mIoContext);
        mpHandler = std::make_unique<EventHandler>(mIoContext, mConfigCache, mpLogger);
    }


    // Passthrough methods

    std::vector<EventHandler::Event> EventHandlerTest::parseDeviceEvents(
        const uint deviceId, const nlohmann::json &events) const {
        return mpHandler->parseDeviceEvents(deviceId, events);
    }

    std::vector<EventHandler::Notification> EventHandlerTest::parseModuleNotifications(
        const uint moduleId, const nlohmann::json &notifications) const {
        return mpHandler->parseModuleNotifications(moduleId, notifications);
    }

    bool EventHandlerTest::shouldEventTrigger(EventHandler::Event &event,
                                              const nlohmann::json &reading,
                                              const nlohmann::json::array_t &deviceExpectedValuesFormat) const {
        return mpHandler->shouldEventTrigger(event, reading, deviceExpectedValuesFormat);
    }

    bool EventHandlerTest::isConditionMet(const nlohmann::json &v, const nlohmann::json &c) const {
        return mpHandler->isConditionMet(v, c);
    }

    EventHandler::Event EventHandlerTest::makeEvent(const uint deviceId, const nlohmann::json &event) {
        return EventHandler::Event(deviceId, event);
    }

    EventHandler::Notification EventHandlerTest::makeNotification(const nlohmann::json &notification) {
        return EventHandler::Notification(notification);
    }


    // Tests

    //region parseDeviceEvents
    TEST_F(EventHandlerTest, ParseDeviceEventsSingleValid) {
        const nlohmann::json events = {
            {
                {"enabled", true},
                {"trigger", "level"},
                {"condition", {{">", 0}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }
        };

        const auto result = parseDeviceEvents(1, events);
        EXPECT_EQ(result.size(), 1);
    }

    TEST_F(EventHandlerTest, ParseDeviceEventsMultipleValid) {
        const nlohmann::json events = {
            {
                {"enabled", true},
                {"trigger", "level"},
                {"condition", {{">", 0}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            },
            {
                {"enabled", false},
                {"trigger", "edge"},
                {"condition", {{"<", 10}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }
        };

        const auto result = parseDeviceEvents(1, events);
        EXPECT_EQ(result.size(), 2);
    }

    TEST_F(EventHandlerTest, ParseDeviceEventsPartiallyInvalid) {
        const nlohmann::json events = {
            {
                {"enabled", true},
                {"trigger", "level"},
                {"condition", {{">", 0}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            },
            {
                // missing enabled
                {"trigger", "edge"},
                {"condition", {{"<", 10}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }
        };

        const auto result = parseDeviceEvents(1, events);
        EXPECT_EQ(result.size(), 1); // only valid event returned
    }

    TEST_F(EventHandlerTest, ParseDeviceEventsAllInvalid) {
        const nlohmann::json events = {
            {
                {"enabled", true},
                // missing Trigger
                {"condition", {{">", 0}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            },
            {
                // missing enabled
                {"trigger", "edge"},
                {"condition", {{"<", 10}}},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }
        };

        const auto result = parseDeviceEvents(1, events);
        EXPECT_TRUE(result.empty());
    }

    TEST_F(EventHandlerTest, ParseDeviceEventsEmpty) {
        const auto result = parseDeviceEvents(1, nlohmann::json::array());
        EXPECT_TRUE(result.empty());
    }

    TEST_F(EventHandlerTest, ParseDeviceEventsNotAnArray) {
        const nlohmann::json events = {{"trigger", "level"}}; // object, not array
        const auto result = parseDeviceEvents(1, events);
        EXPECT_TRUE(result.empty());
    }

    //endregion

    //region parseModuleNotifications
    TEST_F(EventHandlerTest, ParseModuleNotificationsSingleValid) {
        const nlohmann::json notifications = {
            {
                {"enabled", true},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }
        };

        const auto result = parseModuleNotifications(1, notifications);
        EXPECT_EQ(result.size(), 1);
    }

    TEST_F(EventHandlerTest, ParseModuleNotificationsMultipleValid) {
        const nlohmann::json notifications = {
            {
                {"enabled", true},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            },
            {
                {"enabled", false},
                {"action", {{"method", "core.get"}, {"params", "<params_obj>"}}}
            }
        };

        const auto result = parseModuleNotifications(1, notifications);
        EXPECT_EQ(result.size(), 2);
    }

    TEST_F(EventHandlerTest, ParseModuleNotificationsPartiallyInvalid) {
        const nlohmann::json notifications = {
            {
                {"enabled", true},
                {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            },
            {
                {"enabled", true} // missing action
            }
        };

        const auto result = parseModuleNotifications(1, notifications);
        EXPECT_EQ(result.size(), 1);
    }

    TEST_F(EventHandlerTest, ParseModuleNotificationsAllInvalid) {
        const nlohmann::json notifications = {
            {{"enabled", true}}, // missing action
            {{"action", "core.set"}} // wrong action type
        };

        const auto result = parseModuleNotifications(1, notifications);
        EXPECT_TRUE(result.empty());
    }

    TEST_F(EventHandlerTest, ParseModuleNotificationsEmpty) {
        const auto result = parseModuleNotifications(1, nlohmann::json::array());
        EXPECT_TRUE(result.empty());
    }

    TEST_F(EventHandlerTest, ParseModuleNotificationsNotAnArray) {
        const nlohmann::json notifications = {{"enabled", true}}; // object, not array
        const auto result = parseModuleNotifications(1, notifications);
        EXPECT_TRUE(result.empty());
    }

    //endregion

    //region shouldEventTrigger
    TEST_F(EventHandlerTest, ShouldEventTriggerEdgeTrigger) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "edge"},
                                   {"condition", {{">", 0}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {{{"index", 0}, {"label", "test_value"}, {"unit", "N/A"}}};

        EXPECT_TRUE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat)); // ignored
        EXPECT_FALSE(shouldEventTrigger(event, -10, deviceExpectedValuesFormat)); // reset
        EXPECT_TRUE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerLevelTrigger) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{">", 0}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {{{"index", 0}, {"label", "test_value"}, {"unit", "N/A"}}};

        EXPECT_TRUE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
        EXPECT_TRUE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerDisabledEvent) {
        auto event = makeEvent(1, {
                                   {"enabled", false},
                                   {"trigger", "edge"},
                                   {"condition", {{">", 0}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {{{"index", 0}, {"label", "test_value"}, {"unit", "N/A"}}};

        EXPECT_FALSE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerSingularReading) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{">", 0}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {{{"index", 0}, {"label", "test_value"}, {"unit", "N/A"}}};

        EXPECT_TRUE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, -10, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerPrecisionInValuesFormat) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{">", 1.23}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value"}, {"unit", "N/A"}, {"precision", 2}}
        };

        EXPECT_TRUE(shouldEventTrigger(event, 1.235, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, 1.234, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerWrongPrecisionType) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{">", 1.23}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value"}, {"unit", "N/A"}, {"precision", 2.0}}
        };

        EXPECT_FALSE(shouldEventTrigger(event, 1.235, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerPrecisionInValuesFormatWithMultipleValues) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {
                                       "condition", {
                                           {"$0", {{">", 1.23}}},
                                           {"$1", {{"<", 10.1}}},
                                           {"$2", {{"=", 5}}}
                                       }
                                   },
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });

        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}, {"precision", 2}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}, {"precision", 1}},
            {{"index", 2}, {"label", "test_value3"}, {"unit", "N/A"}, {"precision", 0}}
        };

        EXPECT_TRUE(shouldEventTrigger(event, {1.235, 10.04, 5.4}, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, {1.234, 10.04, 5.4}, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, {1.235, 10.05, 5.4}, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, {1.235, 10.04, 5.5}, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerSingularReadingWithIndexInCondition) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{"$0", {{">", 0}}}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });

        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}, {"precision", 2}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}, {"precision", 1}},
            {{"index", 2}, {"label", "test_value3"}, {"unit", "N/A"}, {"precision", 0}}
        };
        EXPECT_TRUE(shouldEventTrigger(event, 20, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, -10, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerMultipleValuesInReading) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{"$0", {{">", 0}}}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}},
            {{"index", 2}, {"label", "test_value3"}, {"unit", "N/A"}}
        };

        EXPECT_TRUE(shouldEventTrigger(event, {20,10,-10}, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, {-10, 10, 20}, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerMultipleValuesInReadingWithMultipleConditions) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {
                                       "condition", {
                                           {"$0", {{">", 0}}},
                                           {"$1", {{"<", 20}}},
                                           {"$2", {{"=", -10}}}
                                       }
                                   },
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}},
            {{"index", 2}, {"label", "test_value3"}, {"unit", "N/A"}}
        };

        EXPECT_TRUE(shouldEventTrigger(event, {20,10,-10}, deviceExpectedValuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, {-10, 10, 20}, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingSizeMismatch) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{"$0", {{">", 0}}}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}},
            {{"index", 2}, {"label", "test_value3"}, {"unit", "N/A"}}
        };

        EXPECT_FALSE(shouldEventTrigger(event, {20,30}, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingInvalidConditionFormat) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{">", 0}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });
        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}},
            {{"index", 2}, {"label", "test_value3"}, {"unit", "N/A"}}
        };

        EXPECT_FALSE(shouldEventTrigger(event, {20,30}, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingNoMatchingConditions) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {"condition", {{">", 0}}},
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });

        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}}
        };

        EXPECT_FALSE(shouldEventTrigger(event, {20, 10}, deviceExpectedValuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingPartialyMatchingConditions) {
        auto event = makeEvent(1, {
                                   {"enabled", true},
                                   {"trigger", "level"},
                                   {
                                       "condition", {
                                           {"$0", {{">", 0}}},
                                           {"$1", "invalid_condition"},
                                       }
                                   },
                                   {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                               });

        const nlohmann::json deviceExpectedValuesFormat = {
            {{"index", 0}, {"label", "test_value1"}, {"unit", "N/A"}},
            {{"index", 1}, {"label", "test_value2"}, {"unit", "N/A"}}
        };

        EXPECT_TRUE(shouldEventTrigger(event, {20, 10}, deviceExpectedValuesFormat));
    }


    //endregion

    // region isConditionMet
    TEST_F(EventHandlerTest, IsConditionMetMultipleConditions) {
        const nlohmann::json conditionRange = {{">", 20}, {"<", 30}};
        EXPECT_TRUE(isConditionMet(25, conditionRange));
        EXPECT_FALSE(isConditionMet(10,conditionRange));
        EXPECT_FALSE(isConditionMet(40, conditionRange));
    }

    TEST_F(EventHandlerTest, IsConditionMetStrictComparison) {
        const nlohmann::json conditionGreater = {{">", 20.2}};
        EXPECT_TRUE(isConditionMet(20.21, conditionGreater));
        EXPECT_FALSE(isConditionMet(20.2, conditionGreater));
        EXPECT_FALSE(isConditionMet(10, conditionGreater));

        const nlohmann::json conditionLess = {{"<", 20.2}};
        EXPECT_FALSE(isConditionMet(20.21, conditionLess));
        EXPECT_FALSE(isConditionMet(20.2, conditionLess));
        EXPECT_TRUE(isConditionMet(10, conditionLess));
    }

    TEST_F(EventHandlerTest, IsConditionMetNonStrictComparison) {
        const nlohmann::json conditionGreater = {{">=", 30}};
        EXPECT_TRUE(isConditionMet(30.1, conditionGreater));
        EXPECT_TRUE(isConditionMet(30, conditionGreater));
        EXPECT_FALSE(isConditionMet(-10, conditionGreater));

        const nlohmann::json conditionLess = {{"<=", 30}};
        EXPECT_FALSE(isConditionMet(30.1, conditionLess));
        EXPECT_TRUE(isConditionMet(30, conditionLess));
        EXPECT_TRUE(isConditionMet(-10, conditionLess));
    }

    TEST_F(EventHandlerTest, IsConditionMetEquality) {
        const nlohmann::json conditionEqual = {{"=", 30.2}};
        EXPECT_TRUE(isConditionMet(30.2, conditionEqual));
        EXPECT_FALSE(isConditionMet(3, conditionEqual));

        const nlohmann::json conditionNotEqual = {{"!=", 30.2}};
        EXPECT_FALSE(isConditionMet(30.2, conditionNotEqual));
        EXPECT_TRUE(isConditionMet(3, conditionNotEqual));
    }

    TEST_F(EventHandlerTest, IsConditionMetStringEquality) {
        const nlohmann::json conditionEqual = {{"=", "qwerty"}};
        EXPECT_TRUE(isConditionMet("qwerty", conditionEqual));
        EXPECT_FALSE(isConditionMet("qwertyu", conditionEqual));

        EXPECT_TRUE(isConditionMet("", {{"=", ""}}));

        const nlohmann::json conditionNotEqual = {{"!=", ""}};
        EXPECT_FALSE(isConditionMet("", conditionNotEqual));
        EXPECT_TRUE(isConditionMet("qwerty", conditionNotEqual));
    }

    TEST_F(EventHandlerTest, IsConditionMetStringContains) {
        EXPECT_TRUE(isConditionMet("qwerty", {{"contains", "qwe"}}));
        EXPECT_TRUE(isConditionMet("qwerty", {{"contains", "wer"}}));
        EXPECT_TRUE(isConditionMet("qwerty", {{"contains", "rty"}}));
        EXPECT_TRUE(isConditionMet("qwerty", {{"contains", "qwerty"}}));
        EXPECT_TRUE(isConditionMet("qwerty", {{"contains", ""}}));

        EXPECT_FALSE(isConditionMet("qwerty", {{"contains", "qry"}}));
        EXPECT_FALSE(isConditionMet("", {{"contains", "qwerty"}}));
    }

    TEST_F(EventHandlerTest, IsConditionMetWrongValueType) {
        const nlohmann::json conditionWrongExpectedValues = {{"<=", {1, 2, 3}}};
        EXPECT_THROW(isConditionMet(1,conditionWrongExpectedValues), std::invalid_argument);

        const nlohmann::json conditionWrongExpectedTypeStr = {{"=", "Test"}};
        EXPECT_THROW(isConditionMet(1,conditionWrongExpectedTypeStr), std::invalid_argument);

        const nlohmann::json conditionWrongExpectedTypeNum = {{"=", 1}};
        EXPECT_THROW(isConditionMet("Test",conditionWrongExpectedTypeNum), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, IsConditionMetEmptyCondition) {
        EXPECT_THROW(isConditionMet(1, nlohmann::json::object()), std::invalid_argument);
        EXPECT_THROW(isConditionMet("test", nlohmann::json::object()), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, IsConditionMetWrongOperator) {
        const nlohmann::json conditionNumeric = {{"contains", 30}};
        EXPECT_THROW(isConditionMet(1,conditionNumeric), std::invalid_argument);

        const nlohmann::json conditionString = {{">", "Test"}};
        EXPECT_THROW(isConditionMet("Test",conditionString), std::invalid_argument);

        const nlohmann::json conditionNoOp = {{"", 30}};
        EXPECT_THROW(isConditionMet(1,conditionNoOp), std::invalid_argument);
    }

    //endregion

    //region Event
    TEST_F(EventHandlerTest, EventValidFormat) {
        EXPECT_NO_THROW(makeEvent(1, {
            {"enabled", true},
            {"trigger", "level"},
            {"condition", {{">", 0}}},
            {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }));
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongEnabled) {
        EXPECT_THROW(makeEvent(1, {
                         {"trigger", "level"},
                         {"condition", {{">", 0}}},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);

        EXPECT_THROW(makeEvent(1, {
                         {"enabled", "true"},
                         {"trigger", "level"},
                         {"condition", {{">", 0}}},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongTrigger) {
        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"condition", {{">", 0}}},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);

        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"trigger", false},
                         {"condition", {{">", 0}}},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsInvalidTriggerValue) {
        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"trigger", "not_existent"},
                         {"condition", {{">", 0}}},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongCondition) {
        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"trigger", "level"},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);

        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"trigger", "level"},
                         {"condition", ">0"},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongAction) {
        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"trigger", "level"},
                         {"condition", {{">", 0}}}
                         }), std::invalid_argument);

        EXPECT_THROW(makeEvent(1, {
                         {"enabled", true},
                         {"trigger", "level"},
                         {"condition", {{">", 0}}},
                         {"action", "core.set"}
                         }), std::invalid_argument);
    }

    //endregion

    //region Notification
    TEST_F(EventHandlerTest, NotificationValidFormat) {
        EXPECT_NO_THROW(makeNotification({
            {"enabled", true},
            {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
            }));
    }

    TEST_F(EventHandlerTest, NotificationRejectsMissingOrWrongEnabled) {
        EXPECT_THROW(makeNotification({
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);

        EXPECT_THROW(makeNotification({
                         {"enabled", "true"},
                         {"action", {{"method", "core.set"}, {"params", "<params_obj>"}}}
                         }), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, NotificationRejectsMissingOrWrongAction) {
        EXPECT_THROW(makeNotification({
                         {"enabled", true}
                         }), std::invalid_argument);

        EXPECT_THROW(makeNotification({
                         {"enabled", true},
                         {"action", "core.set"}
                         }), std::invalid_argument);
    }

    //endregion
}
