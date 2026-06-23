#pragma once

#include <gtest/gtest.h>
#include "event_handler.h"
#include "cache.h"
#include "async_logger.h"

namespace SmartHome {
    class EventHandlerTest : public ::testing::Test {
    protected:
        void SetUp() override;

        std::vector<EventHandler::Event> parseDeviceEvents(uint deviceId, const nlohmann::json &events) const;

        std::vector<EventHandler::Notification> parseModuleNotifications(uint moduleId, const nlohmann::json &notifications) const;

        bool shouldEventTrigger(EventHandler::Event &event,
                                const nlohmann::json &reading,
                                const nlohmann::json::array_t &deviceExpectedValuesFormat) const;

        bool isConditionMet(const nlohmann::json &v, const nlohmann::json &c) const;

        static EventHandler::Event makeEvent(uint deviceId, const nlohmann::json &event);

        static EventHandler::Notification makeNotification(const nlohmann::json &notification);

        ConfigCache mConfigCache; // Not used in testing EventHandler
        ba::io_context mIoContext;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;
        std::unique_ptr<EventHandler> mpHandler;

    };
}
