#pragma once
#include "cache.h"
#include "async_logger.h"
#include "actions/action_helpers.h"

#include <nlohmann/json.hpp>
#include <utility>


namespace SmartHome {
    class EventHandler : public std::enable_shared_from_this<EventHandler> {
        friend class EventHandlerTest;
        using ActionDispatcher =
        std::function<void(std::string_view actionName, uint id, const nlohmann::json &action)>;

    public:
        EventHandler(ba::io_context &ioContext,
                     const ConfigCache &configCache,
                     const ReadingsCache &readingsCache,
                     std::shared_ptr<Utils::AsyncLogger> pLogger,
                     ActionDispatcher dispatchAction = &ActionHelpers::dispatchAutomatedDeviceAction,
                     ActionDispatcher dispatchModuleAction = &ActionHelpers::dispatchAutomatedModuleAction)
            : mIoContext(ioContext),
              mConfigCache(configCache),
              mReadingsCache(readingsCache),
              mpLogger(std::move(pLogger)),
              mDispatchAction(std::move(dispatchAction)),
              mDispatchModuleAction(std::move(dispatchModuleAction)) {
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
            nlohmann::json conditions;
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

        /**
         *
         * @param event
         * @param reading
         * @param deviceExpectedValuesFormat
         * @return
         *
         * @note Parameter \p event might be modified by this function.
         *
         * @pre Caller must hold \c mMutex (exclusive).
         */
        bool shouldEventTrigger(Event &event,
                                const nlohmann::json &reading,
                                const nlohmann::json::array_t &deviceExpectedValuesFormat);

        static bool isConditionMet(const nlohmann::json &value, const nlohmann::json &condition);

        static bool compareNumeric(const nlohmann::json &value, const nlohmann::json &condition);

        static bool compareString(const nlohmann::json &value, const nlohmann::json &condition);

        void dispatchAction(uint deviceId, const nlohmann::json &action) const;

        void dispatchModuleAction(uint moduleId, const nlohmann::json &action) const;

        mutable std::shared_mutex mMutex;

        ba::io_context &mIoContext;
        const ConfigCache &mConfigCache;
        const ReadingsCache &mReadingsCache;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;

        // Dispatch dependencies, used for DI in testing
        ActionDispatcher mDispatchAction;
        ActionDispatcher mDispatchModuleAction;

        /// Device ID: events
        std::map<uint, std::vector<Event> > mDeviceEvents;

        /// Module ID: {notification type: notifications}
        std::map<uint, std::unordered_map<std::string, std::vector<Notification> > > mModuleNotifications;

        std::atomic_bool mIsRunning{false};
    };
}
