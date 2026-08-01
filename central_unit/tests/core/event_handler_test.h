#pragma once
#include "event_handler.h"
#include "cache.h"
#include "async_logger.h"
#include "common/time/testing/fake_time_provider.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    class EventHandlerTest : public ::testing::Test {
    protected:
        struct CapturedDispatch {
            std::string actionName;
            uint id;
            nlohmann::json action;
        };

        void SetUp() override;


        // Passthrough methods
        void loadDevicesEvents() const;

        void loadModulesNotifications() const;

        void handleEvents(uint deviceId) const;

        void handleNotification(uint logicAddress, std::string_view notificationType) const;

        std::vector<EventHandler::Event> parseDeviceEvents(uint deviceId, const nlohmann::json &events) const;

        std::vector<EventHandler::Notification> parseModuleNotifications(
            uint moduleId, const nlohmann::json &notifications) const;

        bool shouldEventTrigger(EventHandler::Event &event,
                                const nlohmann::json &reading,
                                const nlohmann::json::array_t &deviceExpectedValuesFormat);

        static bool isConditionMet(const nlohmann::json &v, const nlohmann::json &c);


        // Helpers
        size_t deviceEventsCount() const;

        bool hasDeviceEvents(uint id) const;

        static nlohmann::json validEvent();

        void seedDevice(uint id, const nlohmann::json &config);

        void seedDeviceWithEventsField(uint id, const nlohmann::json &eventsValue);

        size_t moduleNotificationsCount() const;

        bool hasModuleNotifications(uint moduleId) const;

        bool hasNotificationType(uint moduleId, const std::string &type) const;

        size_t notificationTypeCount(uint moduleId) const;

        static nlohmann::json validNotification();

        void seedModuleWithNotificationField(uint moduleId, uint logicAddress,
                                             const nlohmann::json &onNotificationValue);

        void seedModule(uint moduleId, uint logicAddress, const nlohmann::json &config);

        static nlohmann::json validValuesFormat(size_t count = 1);

        void seedTriggerableDevice(uint deviceId, const nlohmann::json &values);

        static EventHandler::Event makeEvent(uint deviceId, const nlohmann::json &event);

        static EventHandler::Notification makeNotification(const nlohmann::json &notification);


        // Event handler members
        ConfigCache mConfigCache;
        ReadingsCache mReadingsCache{mConfigCache, mTimeProvider};
        ba::io_context mIoContext;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;


        // Members used for testing
        Time::Testing::FakeTimeProvider mTimeProvider{std::chrono::system_clock::now()};
        std::shared_ptr<EventHandler> mpHandler;
        std::vector<CapturedDispatch> mDeviceDispatches;
        std::vector<CapturedDispatch> mModuleDispatches;
    };
}
