#pragma once
#include "cache.h"
#include "async_logger.h"
#include "actions/action_helpers.h"

#include <utility>

#include <nlohmann/json.hpp>

// TODO consider reworking/refactoring EventHandler and its unit tests to remove need for friend class usage.
namespace SmartHome::Tests {
    class EventHandlerTest;
}

namespace SmartHome {
    /**
     * @brief Reacts to device reading changes and module notifications by dispatching configured actions.
     *
     * @details Loads event and notification rules from the config cache, evaluates conditions against live readings, 
     *          and posts matching actions to the Boost.Asio io_context for execution.
     *          Supports edge and level trigger semantics for device events.
     *
     * @note All public methods are thread-safe.
     */
    class EventHandler : public std::enable_shared_from_this<EventHandler> {
        friend class Tests::EventHandlerTest;

    public:
        /**
         * @brief Construct an EventHandler bound to the given caches and io_context.
         *
         * @param ioContext Boost.Asio context for posting actions asynchronously.
         * @param configCache Configuration cache used for device and module config lookup.
         * @param readingsCache Readings cache used to fetch the latest device readings.
         * @param pLogger Logger instance.
         * @param dispatchAction Callable used to dispatch device actions (injectable for testing).
         * @param dispatchModuleAction Callable used to dispatch module actions (injectable for testing).
         */
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

        /**
         * @brief Deconstruct the EventHandler instance. Stops handling new or pending events.
         */
        ~EventHandler();

        EventHandler(const EventHandler &) = delete;

        EventHandler &operator=(const EventHandler &) = delete;

        /**
         * @brief Reload all device events and module notification rules from the config cache.
         *
         * @details Calls \c loadDevicesEvents and \c loadModulesNotifications in sequence.
         *          Existing rules are cleared and rebuilt from the current cache state.
         */
        void loadFromCache();

        /**
         * @brief Reload device event rules from the config cache.
         *
         * @details Clears \c mDeviceEvents and repopulates it by parsing the \c events array from each 
         *          cached device configuration.
         *
         * @note Acquires an exclusive lock on \c mMutex.
         */
        void loadDevicesEvents();

        /**
         * @brief Reload module notification rules from the config cache.
         *
         * @details Clears \c mModuleNotifications and repopulates it by parsing the \c on_notification object 
         *          from each cached module configuration.
         *
         * @note Acquires an exclusive lock on \c mMutex.
         */
        void loadModulesNotifications();

        /**
         * @brief Evaluate all event rules for a device and dispatch matching actions.
         *
         * @details Fetches the latest reading for \p deviceId from \c mReadingsCache,
         *          evaluates each configured event condition, and posts matching actions to the io_context.
         *
         * @param deviceId Device identifier whose events should be evaluated.
         *
         * @note Acquires an exclusive lock on \c mMutex (required by \c shouldEventTrigger).
         */
        void handleEvents(uint deviceId);

        /**
         * @brief Dispatch configured actions for an incoming module notification.
         *
         * @details Looks up module by \p logicAddress and finds all notification rules matching \p notificationType. 
         *          Enabled rules are dispatched via \c dispatchModuleAction.
         *
         * @param logicAddress Module logic address identifying the sender.
         * @param notificationType Notification type string (e.g. \c "alert").
         *
         * @note Acquires a shared lock on \c mMutex.
         */
        void handleNotification(uint logicAddress, std::string_view notificationType);

        /**
         * @brief Start the event handler, enabling event and notification processing.
         */
        void start();

        /**
         * @brief Stop the event handler, preventing further action dispatching.
         */
        void stop();

    private:
        /**
         * @brief Determines how an event re-fires when its condition remains true.
         */
        enum class TriggerType {
            UNDEFINED = 0,
            EDGE, ///< Fires once when condition transitions from false to true
            LEVEL, ///< Fires on every evaluation while condition holds true
        };

        /**
         * @brief A single device event rule parsed from device configuration.
         */
        struct Event {
            // Main structure, stored in device config
            TriggerType trigger;
            nlohmann::json conditions; ///< Conditions object evaluated against device readings
            nlohmann::json action; ///< Action to dispatch when conditions are met
            uint deviceId;
            bool enabled;

            // Metadata
            bool triggered{false}; ///< Used for edge trigger logic

            /**
             * @brief Parse an event rule from a device config JSON object.
             *
             * @param eventDeviceId Owning device identifier.
             * @param event JSON object representing the event rule.
             *
             * @throws std::invalid_argument on missing or invalid required fields.
             */
            explicit Event(uint eventDeviceId, const nlohmann::json &event);
        };

        /**
         * @brief A single module notification rule parsed from module configuration.
         */
        struct Notification {
            nlohmann::json action; ///< Action to dispatch when the notification arrives
            bool enabled;

            /**
             * @brief Parse a notification rule from a module config JSON object.
             *
             * @param notification JSON object representing the notification rule.
             *
             * @throws std::invalid_argument on missing or invalid required fields.
             */
            explicit Notification(const nlohmann::json &notification);
        };

        /**
         * @brief Parse an array of event rules from device configuration.
         *
         * @param deviceId Device identifier used for logging.
         * @param events JSON array of event objects.
         *
         * @return Vector of successfully parsed \c Event instances (failed entries are skipped).
         */
        std::vector<Event> parseDeviceEvents(uint deviceId, const nlohmann::json &events) const;

        /**
         * @brief Parse an array of notification rules from module configuration.
         *
         * @param moduleId Module identifier used for logging.
         * @param notifications JSON array of notification objects.
         *
         * @return Vector of successfully parsed \c Notification instances (failed entries are skipped).
         */
        std::vector<Notification> parseModuleNotifications(uint moduleId, const nlohmann::json &notifications) const;

        /**
         * @brief Checks if the event should trigger based on the current reading.
         *
         * @param event \c Event to check.
         * @param reading Reading from the device.
         * @param deviceExpectedValuesFormat Device expected values format.
         *
         * @return \c true if \c Event conditions are met, \c false otherwise or on error.
         *
         * @note Parameter \p event will be modified by this function depending on its trigger type.
         *       If \c TriggerType::EDGE is selected then \c triggered field will be toggled accordingly to edge logic.
         *
         * @note With an array of readings all the conditions must be fulfilled (AND logic).
         *
         * @pre Caller must hold \c mMutex (exclusive).
         */
        bool shouldEventTrigger(Event &event,
                                const nlohmann::json &reading,
                                const nlohmann::json::array_t &deviceExpectedValuesFormat);

        /**
         * @brief Checks if conditions are met for provided value.
         *
         * @details Calls \c compareNumeric or \c compareString depending on the type of \p value.
         *
         * @param value Reading value to check.
         * @param conditions Object with conditions to check against.
         *
         * @return \c compareNumeric or \c compareString result.
         *
         * @throws std::invalid_argument if \p value is not of expected type.
         * @throws std::invalid_argument if \p condition is invalid or empty.
         * @throws std::logic_error on unexpected error (unreachable in normal operation).
         */
        static bool isConditionMet(const nlohmann::json &value, const nlohmann::json &conditions);

        /**
         * @brief Compares numeric values against conditions.
         *
         * @param value Numeric value to compare.
         * @param conditions Conditions to check against.
         *
         * @return \c true if all conditions are met (AND logic), \c false otherwise.
         *
         * @throws std::invalid_argument if conditions are invalid or empty.
         */
        static bool compareNumeric(const nlohmann::json &value, const nlohmann::json &conditions);

        /**
         * @brief Compares string values against conditions.
         *
         * @param value String value to compare.
         * @param conditions Conditions to check against.
         *
         * @return \c true if all conditions are met (AND logic), \c false otherwise.
         *
         * @throws std::invalid_argument if conditions are invalid or empty.
         */
        static bool compareString(const nlohmann::json &value, const nlohmann::json &conditions);

        /**
         * @brief Post a device action to the io_context for async execution.
         *
         * @param deviceId Device identifier passed to the action dispatcher.
         * @param action Action JSON payload.
         */
        void dispatchAction(uint deviceId, const nlohmann::json &action) const;

        /**
         * @brief Post a module action to the io_context for async execution.
         *
         * @param moduleId Module identifier passed to the action dispatcher.
         * @param action Action JSON payload.
         */
        void dispatchModuleAction(uint moduleId, const nlohmann::json &action) const;

        mutable std::shared_mutex mMutex;

        ba::io_context &mIoContext;
        const ConfigCache &mConfigCache;
        const ReadingsCache &mReadingsCache;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;

        ActionDispatcher mDispatchAction; ///< Device action dispatcher (injectable for testing)
        ActionDispatcher mDispatchModuleAction; ///< Module action dispatcher (injectable for testing)

        /// Device ID: events
        std::map<uint, std::vector<Event> > mDeviceEvents;

        /// Module ID: {notification type: notifications}
        std::map<uint, std::unordered_map<std::string, std::vector<Notification> > > mModuleNotifications;

        std::atomic_bool mIsRunning{false};
    };
}
