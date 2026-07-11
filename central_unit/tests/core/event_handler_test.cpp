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

        auto deviceFakeDispatch = [this](const std::string_view name, const uint id, const nlohmann::json &action) {
            mDeviceDispatches.push_back({std::string(name), id, action});
        };
        auto moduleFakeDispatch = [this](const std::string_view name, const uint id, const nlohmann::json &action) {
            mModuleDispatches.push_back({std::string(name), id, action});
        };

        mpLogger = std::make_shared<Utils::AsyncLogger>(logger, mIoContext);
        mpHandler = std::make_shared<EventHandler>(
            mIoContext, mConfigCache, mReadingsCache, mpLogger, deviceFakeDispatch, moduleFakeDispatch);

        mpHandler->start();
    }


    //region Passthrough methods
    void EventHandlerTest::loadDevicesEvents() const {
        mpHandler->loadDevicesEvents();
    }

    void EventHandlerTest::loadModulesNotifications() const {
        mpHandler->loadModulesNotifications();
    }

    void EventHandlerTest::handleEvents(const uint deviceId) const {
        mpHandler->handleEvents(deviceId);
    }

    void EventHandlerTest::handleNotification(const uint logicAddress, const std::string_view notificationType) const {
        mpHandler->handleNotification(logicAddress, notificationType);
    }

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
                                              const nlohmann::json::array_t &deviceExpectedValuesFormat) {
        std::unique_lock lock(mpHandler->mMutex);
        return mpHandler->shouldEventTrigger(event, reading, deviceExpectedValuesFormat);
    }

    bool EventHandlerTest::isConditionMet(const nlohmann::json &v, const nlohmann::json &c) {
        return EventHandler::isConditionMet(v, c);
    }

    //endregion

    //region Helpers
    size_t EventHandlerTest::deviceEventsCount() const {
        return mpHandler->mDeviceEvents.size();
    }

    bool EventHandlerTest::hasDeviceEvents(const uint id) const {
        return mpHandler->mDeviceEvents.contains(id);
    }

    nlohmann::json EventHandlerTest::validEvent() {
        return {
            {"enabled", true},
            {"trigger", "level"},
            {"condition", {{">", 0}}},
            {"action", {{"method", "core.set"}, {"params", nlohmann::json::object()}}}
        };
    }

    void EventHandlerTest::seedDevice(const uint id, const nlohmann::json &config) {
        CachedDevice device;
        device.id = id;
        device.logicId = id;
        device.moduleId = 1;
        device.name = "name";
        device.type = "sensor";
        device.config = config;
        mConfigCache.setDevice(device);
    }

    void EventHandlerTest::seedDeviceWithEventsField(const uint id, const nlohmann::json &eventsValue) {
        nlohmann::json deviceConfig;
        deviceConfig["events"] = eventsValue;
        seedDevice(id, deviceConfig);
    }

    size_t EventHandlerTest::moduleNotificationsCount() const {
        return mpHandler->mModuleNotifications.size();
    }

    bool EventHandlerTest::hasModuleNotifications(const uint moduleId) const {
        return mpHandler->mModuleNotifications.contains(moduleId);
    }

    bool EventHandlerTest::hasNotificationType(const uint moduleId, const std::string &type) const {
        const auto it = mpHandler->mModuleNotifications.find(moduleId);
        return it != mpHandler->mModuleNotifications.end() && it->second.contains(type);
    }

    size_t EventHandlerTest::notificationTypeCount(const uint moduleId) const {
        const auto it = mpHandler->mModuleNotifications.find(moduleId);
        return it == mpHandler->mModuleNotifications.end() ? 0 : it->second.size();
    }

    nlohmann::json EventHandlerTest::validNotification() {
        return {
            {"enabled", true},
            {"action", {{"method", "core.get"}, {"params", nlohmann::json::object()}}}
        };
    }

    void EventHandlerTest::seedModuleWithNotificationField(const uint moduleId,
                                                           const uint logicAddress,
                                                           const nlohmann::json &onNotificationValue) {
        nlohmann::json moduleConfig;
        moduleConfig[std::string("on_notification")] = onNotificationValue;
        seedModule(moduleId, logicAddress, moduleConfig);
    }

    void EventHandlerTest::seedModule(const uint moduleId, const uint logicAddress, const nlohmann::json &config) {
        CachedModule module;
        module.id = moduleId;
        module.logicAddress = logicAddress;
        module.name = "name";
        module.config = config;
        mConfigCache.setModule(module);
    }

    nlohmann::json EventHandlerTest::validValuesFormat(const size_t count) {
        nlohmann::json valuesFormat = nlohmann::json::array();
        for (size_t i = 0; i < count; ++i) {
            valuesFormat.push_back({
                {"index", i},
                {"label", "test_value" + std::to_string(i)},
                {"unit", "N/A"}
            });
        }

        return valuesFormat;
    }

    void EventHandlerTest::seedTriggerableDevice(const uint deviceId, const nlohmann::json &values) {
        nlohmann::json deviceConfig;
        deviceConfig["events"] = nlohmann::json::array({validEvent()});
        deviceConfig["values"] = values;
        seedDevice(deviceId, deviceConfig);
    }

    EventHandler::Event EventHandlerTest::makeEvent(const uint deviceId, const nlohmann::json &event) {
        return EventHandler::Event(deviceId, event);
    }

    EventHandler::Notification EventHandlerTest::makeNotification(const nlohmann::json &notification) {
        return EventHandler::Notification(notification);
    }

    //endregion


    // Tests

    //region loadDevicesEvents
    TEST_F(EventHandlerTest, LoadDevicesEventsNoDevices) {
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 0);
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsSingleDeviceWithEvents) {
        seedDeviceWithEventsField(1, nlohmann::json::array({validEvent()}));
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 1);
        EXPECT_TRUE(hasDeviceEvents(1));
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsMultipleDevicesWithEvents) {
        seedDeviceWithEventsField(1, nlohmann::json::array({validEvent()}));
        seedDeviceWithEventsField(2, nlohmann::json::array({validEvent()}));
        seedDeviceWithEventsField(3, nlohmann::json::array({validEvent()}));
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 3);
        EXPECT_TRUE(hasDeviceEvents(1));
        EXPECT_TRUE(hasDeviceEvents(2));
        EXPECT_TRUE(hasDeviceEvents(3));
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsSkipsDeviceWithoutEventsField) {
        seedDevice(1, nlohmann::json::object());
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 0);
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsSkipsDeviceWithNonArrayEvents) {
        seedDeviceWithEventsField(1, "not_an_array");
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 0);
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsSkipsDeviceWithEmptyEventsArray) {
        seedDeviceWithEventsField(1, nlohmann::json::array());
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 0);
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsLoadsOnlyDevicesWithValidEvents) {
        seedDeviceWithEventsField(1, nlohmann::json::array({validEvent()}));
        seedDevice(2, nlohmann::json::object());
        seedDeviceWithEventsField(3, nlohmann::json::array({validEvent()}));
        loadDevicesEvents();
        EXPECT_EQ(deviceEventsCount(), 2);
        EXPECT_TRUE(hasDeviceEvents(1));
        EXPECT_FALSE(hasDeviceEvents(2));
        EXPECT_TRUE(hasDeviceEvents(3));
    }

    TEST_F(EventHandlerTest, LoadDevicesEventsClearsPreviousOnReload) {
        seedDeviceWithEventsField(1, nlohmann::json::array({validEvent()}));
        loadDevicesEvents();
        EXPECT_TRUE(hasDeviceEvents(1));

        mConfigCache.eraseDevice(1);
        loadDevicesEvents();
        EXPECT_FALSE(hasDeviceEvents(1));
        EXPECT_EQ(deviceEventsCount(), 0);
    }

    //endregion

    //region loadModulesNotifications
    TEST_F(EventHandlerTest, LoadModulesNotificationsNoModules) {
        loadModulesNotifications();
        EXPECT_EQ(moduleNotificationsCount(), 0);
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsMultipleModules) {
        seedModuleWithNotificationField(1, 11, {{"alert", nlohmann::json::array({validNotification()})}});
        seedModuleWithNotificationField(2, 22, {{"alert", nlohmann::json::array({validNotification()})}});
        loadModulesNotifications();

        EXPECT_EQ(moduleNotificationsCount(), 2);
        EXPECT_TRUE(hasModuleNotifications(1));
        EXPECT_TRUE(hasModuleNotifications(2));
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsGroupsByType) {
        seedModuleWithNotificationField(1, 11, {
                                            {"manual_trigger", nlohmann::json::array({validNotification()})},
                                            {
                                                "power_loss",
                                                nlohmann::json::array({validNotification(), validNotification()})
                                            }
                                        });
        loadModulesNotifications();

        EXPECT_EQ(notificationTypeCount(1), 2);
        EXPECT_TRUE(hasNotificationType(1, "manual_trigger"));
        EXPECT_TRUE(hasNotificationType(1, "power_loss"));
        EXPECT_FALSE(hasNotificationType(1, "alert"));
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsIndexesLogicAddressEvenWithoutNotifications) {
        seedModule(1, 11, nlohmann::json::object());
        loadModulesNotifications();

        EXPECT_FALSE(hasModuleNotifications(1)); // no notifications in map
        EXPECT_TRUE(mConfigCache.getModule(1).has_value());
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsSkipsNonObjectNotificationField) {
        seedModuleWithNotificationField(1, 11, "not_an_object");
        loadModulesNotifications();

        EXPECT_FALSE(hasModuleNotifications(1));
        EXPECT_TRUE(mConfigCache.getModule(1).has_value());
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsSkipsEmptyTypeArray) {
        seedModuleWithNotificationField(1, 11, {
                                            {"manual_trigger", nlohmann::json::array()}
                                        });
        loadModulesNotifications();

        EXPECT_FALSE(hasModuleNotifications(1));
        EXPECT_TRUE(mConfigCache.getModule(1).has_value());
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsKeepsValidTypeDropsEmpty) {
        seedModuleWithNotificationField(2, 22, {
                                            {"alert", nlohmann::json::array({validNotification()})},
                                            {"manual_trigger", nlohmann::json::array()}
                                        });
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(2));
        EXPECT_EQ(notificationTypeCount(2), 1);
        EXPECT_TRUE(hasNotificationType(2, "alert"));
        EXPECT_FALSE(hasNotificationType(2, "manual_trigger"));
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsClearsPreviousOnReload) {
        seedModuleWithNotificationField(1, 11, {{"alert", nlohmann::json::array({validNotification()})}});
        loadModulesNotifications();
        EXPECT_TRUE(hasModuleNotifications(1));
        EXPECT_TRUE(mConfigCache.getModule(1).has_value());


        mConfigCache.eraseModule(1);
        loadModulesNotifications();
        EXPECT_FALSE(hasModuleNotifications(1));
        EXPECT_FALSE(mConfigCache.getModule(1).has_value());
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsSkipsUnknownType) {
        seedModuleWithNotificationField(1, 11, {
                                            {"not_a_real_type", nlohmann::json::array({validNotification()})}
                                        });
        loadModulesNotifications();

        EXPECT_FALSE(hasModuleNotifications(1));
        EXPECT_TRUE(mConfigCache.getModule(1).has_value());
    }

    TEST_F(EventHandlerTest, LoadModulesNotificationsKeepsKnownTypeDropsUnknown) {
        seedModuleWithNotificationField(2, 22, {
                                            {"manual_trigger", nlohmann::json::array({validNotification()})},
                                            {"alert", nlohmann::json::array({validNotification()})},
                                            {"power_loss", nlohmann::json::array({validNotification()})},
                                            {"not_a_real_type", nlohmann::json::array({validNotification()})}
                                        });
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(2));
        EXPECT_EQ(notificationTypeCount(2), 3);
        EXPECT_TRUE(hasNotificationType(2, "manual_trigger"));
        EXPECT_TRUE(hasNotificationType(2, "alert"));
        EXPECT_TRUE(hasNotificationType(2, "power_loss"));
        EXPECT_FALSE(hasNotificationType(2, "not_a_real_type"));
    }

    //endregion

    //region handleEvents
    TEST_F(EventHandlerTest, HandleEvents) {
        constexpr uint deviceId = 1;
        nlohmann::json deviceConfig;
        deviceConfig["events"] = nlohmann::json::array({validEvent(), validEvent(), validEvent()});
        deviceConfig["values"] = validValuesFormat();
        seedDevice(deviceId, deviceConfig);
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 3);
        EXPECT_EQ(mDeviceDispatches.front().action, validEvent()["action"]);
        EXPECT_EQ(mDeviceDispatches.front().actionName, Constants::AutomatedActionNames::CONDITIONAL_EVENT);
        EXPECT_EQ(mDeviceDispatches.front().id, deviceId);
    }

    TEST_F(EventHandlerTest, HandleEventWithConditionNotMet) {
        constexpr uint deviceId = 1;
        seedTriggerableDevice(deviceId, validValuesFormat());
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, -5, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventHandlerStopped) {
        constexpr uint deviceId = 1;
        seedTriggerableDevice(deviceId, validValuesFormat());
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        mpHandler->stop();
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventNoDeviceEvents) {
        constexpr uint deviceId = 1;
        nlohmann::json deviceConfig;
        deviceConfig["events"] = {};
        deviceConfig["values"] = validValuesFormat();
        seedDevice(deviceId, deviceConfig);
        loadDevicesEvents();

        EXPECT_FALSE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventNoCachedReadings) {
        constexpr uint deviceId = 1;
        seedTriggerableDevice(deviceId, validValuesFormat());
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventNoDeviceConfig) {
        constexpr uint deviceId = 1;
        seedTriggerableDevice(deviceId, validValuesFormat());
        loadDevicesEvents();
        mConfigCache.clear();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventNoExpectedValuesInConfig) {
        constexpr uint deviceId = 1;
        nlohmann::json deviceConfig;
        deviceConfig["events"] = {validEvent()};
        seedDevice(deviceId, deviceConfig);
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventInvalidExpectedValuesInConfig) {
        constexpr uint deviceId = 1;
        seedTriggerableDevice(deviceId, "not_an_array");
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleEventEmptyExpectedValuesInConfig) {
        constexpr uint deviceId = 1;
        seedTriggerableDevice(deviceId, nlohmann::json::array());
        loadDevicesEvents();

        EXPECT_TRUE(hasDeviceEvents(deviceId));

        mReadingsCache.set(deviceId, 10, {});
        handleEvents(deviceId);
        mIoContext.poll();

        EXPECT_EQ(mDeviceDispatches.size(), 0);
    }


    //endregion

    //region handleNotification
    TEST_F(EventHandlerTest, HandleNotifications) {
        constexpr uint moduleId = 2;
        constexpr uint logicAddress = 22;
        seedModuleWithNotificationField(moduleId, logicAddress, {
                                            {"alert", {validNotification(), validNotification(), validNotification()}}
                                        });
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(moduleId));

        handleNotification(logicAddress, "alert");
        mIoContext.poll();

        EXPECT_EQ(mModuleDispatches.size(), 3);
        EXPECT_EQ(mModuleDispatches.front().action, validNotification()["action"]);
        EXPECT_EQ(mModuleDispatches.front().actionName, Constants::AutomatedActionNames::MODULE_NOTIFICATION);
        EXPECT_EQ(mModuleDispatches.front().id, moduleId);
    }

    TEST_F(EventHandlerTest, HandleNotificationDisabledNotification) {
        constexpr uint moduleId = 2;
        constexpr uint logicAddress = 22;
        nlohmann::json disabledNotification = validNotification();
        disabledNotification["enabled"] = false;
        seedModuleWithNotificationField(moduleId, logicAddress, {{"alert", {disabledNotification}}});
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(moduleId));

        handleNotification(logicAddress, "alert");
        mIoContext.poll();

        EXPECT_EQ(mModuleDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleNotificationHandlerStopped) {
        constexpr uint moduleId = 2;
        constexpr uint logicAddress = 22;
        seedModuleWithNotificationField(moduleId, logicAddress, {{"alert", {validNotification()}}});
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(moduleId));

        mpHandler->stop();
        handleNotification(logicAddress, "alert");
        mIoContext.poll();

        EXPECT_EQ(mModuleDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleNotificationInvalidLogicAddress) {
        constexpr uint moduleId = 2;
        constexpr uint logicAddress = 22;
        seedModuleWithNotificationField(moduleId, logicAddress, {{"alert", {validNotification()}}});
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(moduleId));

        handleNotification(logicAddress + 1, "alert");
        mIoContext.poll();

        EXPECT_EQ(mModuleDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleNotificationNoModuleNotificationsConfigured) {
        constexpr uint moduleId = 2;
        constexpr uint logicAddress = 22;
        seedModuleWithNotificationField(moduleId, logicAddress, {});
        loadModulesNotifications();

        EXPECT_FALSE(hasModuleNotifications(moduleId));

        handleNotification(logicAddress, "alert");
        mIoContext.poll();

        EXPECT_EQ(mModuleDispatches.size(), 0);
    }

    TEST_F(EventHandlerTest, HandleNotificationNoMatchingNotificationsOfType) {
        constexpr uint moduleId = 2;
        constexpr uint logicAddress = 22;
        seedModuleWithNotificationField(moduleId, logicAddress, {{"manual_trigger", {validNotification()}}});
        loadModulesNotifications();

        EXPECT_TRUE(hasModuleNotifications(moduleId));

        handleNotification(logicAddress, "alert");
        mIoContext.poll();

        EXPECT_EQ(mModuleDispatches.size(), 0);
    }

    //endregion

    //region parseDeviceEvents
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
        auto eventJson = validEvent();
        eventJson["trigger"] = "edge";
        auto event = makeEvent(1, eventJson);

        EXPECT_TRUE(shouldEventTrigger(event, 20, validValuesFormat()));
        EXPECT_FALSE(shouldEventTrigger(event, 20, validValuesFormat())); // ignored
        EXPECT_FALSE(shouldEventTrigger(event, -10, validValuesFormat())); // reset
        EXPECT_TRUE(shouldEventTrigger(event, 20, validValuesFormat()));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerLevelTrigger) {
        auto eventJson = validEvent();
        eventJson["trigger"] = "level";
        auto event = makeEvent(1, eventJson);

        EXPECT_TRUE(shouldEventTrigger(event, 20, validValuesFormat()));
        EXPECT_TRUE(shouldEventTrigger(event, 20, validValuesFormat()));
        EXPECT_FALSE(shouldEventTrigger(event, -10, validValuesFormat()));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerDisabledEvent) {
        auto eventJson = validEvent();
        eventJson["enabled"] = false;
        auto event = makeEvent(1, eventJson);

        EXPECT_FALSE(shouldEventTrigger(event, 20, validValuesFormat()));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerPrecisionInValuesFormat) {
        auto eventJson = validEvent();
        eventJson["condition"] = {{">", 1.23}};
        auto event = makeEvent(1, eventJson);

        auto valuesFormat = validValuesFormat();
        valuesFormat.front()["precision"] = 2;

        EXPECT_TRUE(shouldEventTrigger(event, 1.235, valuesFormat));
        EXPECT_FALSE(shouldEventTrigger(event, 1.234, valuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerWrongPrecisionType) {
        auto eventJson = validEvent();
        eventJson["condition"] = {{">", 1.23}};
        auto event = makeEvent(1, eventJson);

        auto valuesFormat = validValuesFormat();
        valuesFormat.front()["precision"] = 2.0;

        EXPECT_FALSE(shouldEventTrigger(event, 1.235, valuesFormat));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerSingularReadingWithIndexInCondition) {
        auto eventJson = validEvent();
        eventJson["condition"] = {{"$0", {{">", 0}}}};
        auto event = makeEvent(1, eventJson);

        EXPECT_TRUE(shouldEventTrigger(event, 20, validValuesFormat(3)));
        EXPECT_FALSE(shouldEventTrigger(event, -10, validValuesFormat(3)));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerMultipleValuesInReading) {
        auto eventJson = validEvent();
        eventJson["condition"] = {{"$0", {{">", 0}}}};
        auto event = makeEvent(1, eventJson);

        EXPECT_TRUE(shouldEventTrigger(event, {20,10,-10}, validValuesFormat(3)));
        EXPECT_FALSE(shouldEventTrigger(event, {-10, 10, 20}, validValuesFormat(3)));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerMultipleValuesInReadingWithMultipleConditions) {
        auto eventJson = validEvent();
        eventJson["condition"] = {
            {"$0", {{">", 0}}},
            {"$1", {{"<", 20}}},
            {"$2", {{"=", -10}}}
        };
        auto event = makeEvent(1, eventJson);

        EXPECT_TRUE(shouldEventTrigger(event, {20,10,-10}, validValuesFormat(3)));
        EXPECT_FALSE(shouldEventTrigger(event, {-10, 10, 20}, validValuesFormat(3)));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingSizeMismatch) {
        auto eventJson = validEvent();
        eventJson["condition"] = {{"$0", {{">", 0}}}};
        auto event = makeEvent(1, eventJson);

        EXPECT_FALSE(shouldEventTrigger(event, {20,30}, validValuesFormat(3)));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingNoMatchingConditions) {
        auto event = makeEvent(1, validEvent());

        EXPECT_FALSE(shouldEventTrigger(event, {20, 10}, validValuesFormat(2)));
    }

    TEST_F(EventHandlerTest, ShouldEventTriggerArrayReadingPartialyMatchingConditions) {
        auto eventJson = validEvent();
        eventJson["condition"] = {
            {"$2", {{">", 0}}},
            {"$1", "invalid_condition"},
        };
        auto event = makeEvent(1, eventJson);

        EXPECT_TRUE(shouldEventTrigger(event, {-20, -10, 30}, validValuesFormat(3)));
        EXPECT_FALSE(shouldEventTrigger(event, {-20, -10, -30}, validValuesFormat(3)));
    }


    //endregion

    // region isConditionMet
    TEST_F(EventHandlerTest, IsConditionMetEvaluates) {
        struct Case {
            nlohmann::json value;
            nlohmann::json condition;
            bool expected;
        };

        const std::vector<Case> cases = {
            // multiple conditions (range)
            {25, {{">", 20}, {"<", 30}}, true},
            {10, {{">", 20}, {"<", 30}}, false},
            {40, {{">", 20}, {"<", 30}}, false},

            // strict comparison
            {20.21, {{">", 20.2}}, true},
            {20.2, {{">", 20.2}}, false},
            {10, {{">", 20.2}}, false},
            {20.21, {{"<", 20.2}}, false},
            {20.2, {{"<", 20.2}}, false},
            {10, {{"<", 20.2}}, true},

            // non-strict comparison
            {30.1, {{">=", 30}}, true},
            {30, {{">=", 30}}, true},
            {-10, {{">=", 30}}, false},
            {30.1, {{"<=", 30}}, false},
            {30, {{"<=", 30}}, true},
            {-10, {{"<=", 30}}, true},

            // numeric equality
            {30.2, {{"=", 30.2}}, true},
            {3, {{"=", 30.2}}, false},
            {30.2, {{"!=", 30.2}}, false},
            {3, {{"!=", 30.2}}, true},

            // string equality
            {"qwerty", {{"=", "qwerty"}}, true},
            {"qwertyu", {{"=", "qwerty"}}, false},
            {"", {{"=", ""}}, true},
            {"", {{"!=", ""}}, false},
            {"qwerty", {{"!=", ""}}, true},

            // string contains
            {"qwerty", {{"contains", "qwe"}}, true},
            {"qwerty", {{"contains", "wer"}}, true},
            {"qwerty", {{"contains", "rty"}}, true},
            {"qwerty", {{"contains", "qwerty"}}, true},
            {"qwerty", {{"contains", ""}}, true},
            {"qwerty", {{"contains", "qry"}}, false},
            {"", {{"contains", "qwerty"}}, false},
        };

        size_t i = 0;
        for (const auto &[value, condition, expected]: cases) {
            SCOPED_TRACE("case #"+ std::to_string(i++) + " value=" + value.dump() + " condition=" + condition.dump());
            EXPECT_EQ(isConditionMet(value, condition), expected);
        }
    }

    TEST_F(EventHandlerTest, IsConditionMetThrows) {
        struct Case {
            nlohmann::json value;
            nlohmann::json condition;
        };

        const std::vector<Case> cases = {
            // wrong value type
            {true, {{"<=", 1}}}, // bool as value

            // wrong expected value type
            {1, {{"<=", {1, 2, 3}}}}, // array as expected value
            {1, {{"=", "Test"}}}, // string expected vs numeric value
            {"Test", {{"=", 1}}}, // numeric expected vs string value

            // empty condition
            {1, nlohmann::json::object()},
            {"test", nlohmann::json::object()},

            // wrong operator for type
            {1, {{"contains", 30}}}, // contains on numeric
            {"Test", {{">", "Test"}}}, // greater on string
            {1, {{"", 30}}}, // empty operator
        };

        size_t i = 0;
        for (const auto &[value, condition]: cases) {
            SCOPED_TRACE("case #"+ std::to_string(i++) + " value=" + value.dump() + " cond=" + condition.dump());
            EXPECT_THROW(isConditionMet(value, condition), std::invalid_argument);
        }
    }

    //endregion

    //region Event
    TEST_F(EventHandlerTest, EventValidFormat) {
        EXPECT_NO_THROW(makeEvent(1, validEvent()));
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongEnabled) {
        auto event = validEvent();

        event.erase("enabled");
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);

        event["enabled"] = "true";
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongTrigger) {
        auto event = validEvent();

        event.erase("trigger");
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);

        event["trigger"] = false;
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsInvalidTriggerValue) {
        auto event = validEvent();

        event["trigger"] = "non_existent";
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongCondition) {
        auto event = validEvent();

        event.erase("condition");
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);

        event["condition"] = ">0";
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, EventRejectsMissingOrWrongAction) {
        auto event = validEvent();

        event.erase("action");
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);

        event["action"] = "core.set";
        EXPECT_THROW(makeEvent(1, event), std::invalid_argument);
    }

    //endregion

    //region Notification
    TEST_F(EventHandlerTest, NotificationValidFormat) {
        EXPECT_NO_THROW(makeNotification(validNotification()));
    }

    TEST_F(EventHandlerTest, NotificationRejectsMissingOrWrongEnabled) {
        auto notification = validNotification();

        notification.erase("enabled");
        EXPECT_THROW(makeNotification(notification), std::invalid_argument);

        notification["enabled"] = "true";
        EXPECT_THROW(makeNotification(notification), std::invalid_argument);
    }

    TEST_F(EventHandlerTest, NotificationRejectsMissingOrWrongAction) {
        auto notification = validNotification();

        notification.erase("action");
        EXPECT_THROW(makeNotification(notification), std::invalid_argument);

        notification["action"] = "core.set";
        EXPECT_THROW(makeNotification(notification), std::invalid_argument);
    }

    //endregion
}
