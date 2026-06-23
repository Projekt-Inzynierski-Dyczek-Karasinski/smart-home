#pragma once
#include "cache.h"
#include "async_logger.h"
#include "actions/action_helpers.h"

#include <nlohmann/json.hpp>
#include <utility>


namespace SmartHome {
    class EventHandler {
        friend class EventHandlerTest;

    public:
        EventHandler(ba::io_context &ioContext,
                     const ConfigCache &configCache,
                     std::shared_ptr<Utils::AsyncLogger> pLogger)
            : mIoContext(ioContext),
              mConfigCache(configCache),
              mpLogger(std::move(pLogger)) {
        }

        ~EventHandler();

        void loadFromCache();

        void loadDevicesEvents();

        void loadModulesNotifications();

        void handleEvents(uint deviceId);

        void handleNotification(uint logicAddress, std::string_view notificationType);

        void start();

        void stop();

    private:
        enum class TriggerType {
            UNDEFINED = 0,
            EDGE,
            LEVEL,
        };

        struct Event {
            // Main structure, stored in device config
            TriggerType trigger;
            nlohmann::json condition;
            nlohmann::json action;
            uint deviceId;
            bool enabled;

            // Metadata
            bool triggered{false}; /// Used for edge trigger

            explicit Event(uint eventDeviceId, const nlohmann::json &event);
        };

        struct Notification {
            // Stored in module config
            nlohmann::json action;
            bool enabled;

            explicit Notification(const nlohmann::json &notification);
        };

        std::vector<Event> parseDeviceEvents(uint deviceId, const nlohmann::json &events) const;

        std::vector<Notification> parseModuleNotifications(uint moduleId, const nlohmann::json &notifications) const;

        bool shouldEventTrigger(Event &event,
                                const nlohmann::json &reading,
                                const nlohmann::json::array_t &deviceExpectedValuesFormat) const;

        bool isConditionMet(const nlohmann::json &value, const nlohmann::json &condition) const;

        bool compareNumeric(const nlohmann::json &value, const nlohmann::json &condition) const;

        bool compareString(const nlohmann::json &value, const nlohmann::json &condition) const;

        void dispatchAction(uint deviceId, const nlohmann::json& action) const;

        void dispatchModuleAction(uint moduleId, const nlohmann::json& action) const;

        mutable std::shared_mutex mMutex;

        ba::io_context &mIoContext;
        const ConfigCache &mConfigCache;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;

        /// Device ID: events
        std::map<uint, std::vector<Event>> mDeviceEvents;

        /// Module ID: {notification type: notifications}
        std::map<uint, std::unordered_map<std::string, std::vector<Notification>>> mModuleNotifications;
        std::map<uint, uint> mLogicAddressToModuleId;

        std::atomic_bool mIsRunning{false};
        std::atomic_bool mIsStopping{false};
    };
}
