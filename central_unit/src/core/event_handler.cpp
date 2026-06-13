#include "event_handler.h"
#include "core.h"
#include "constants.h"
#include "actions/database_actions.h"

namespace SmartHome {
    using namespace std::string_literals;

    namespace cdck = Constants::DeviceConfigKeys;
    namespace cmck = Constants::ModuleConfigKeys;

    EventHandler::~EventHandler() {
        stop();
    }

    void EventHandler::loadFromCache() {
        loadDevicesEvents();
        loadModulesNotifications();
    }

    void EventHandler::loadDevicesEvents() {
        mpLogger->debug("[EVENT_HANDLER] [LOAD_DEVICES_EVENTS] Called");
        std::unique_lock lock(mMutex);

        mDeviceEvents.clear();

        const auto cachedDevices = Core::Instance().configCache().getAllDevices();
        if (cachedDevices.empty()) {
            mpLogger->warning("[EVENT_HANDLER] [LOAD_DEVICES_EVENTS] No cached devices found");
            return;
        }

        uint devicesWithEvents = 0;
        for (const auto &device: cachedDevices) {
            if (!device.config.contains(cdck::EVENTS)) continue; // Ignore devices without events
            if (!device.config.at(cdck::EVENTS).is_object()) {
                mpLogger->errorf(
                    "[EVENT_HANDLER] [LOAD_DEVICES_EVENTS] Device [%u] has invalid 'events' field in config, "
                    "it must be an object",
                    device.id);
                continue;
            }

            auto deviceId = device.id;
            auto events = device.config.at(cdck::EVENTS);

            auto deviceEvents = parseDeviceEvents(deviceId, events);
            if (deviceEvents.empty()) continue;

            devicesWithEvents++;
            mDeviceEvents.emplace(deviceId, std::move(deviceEvents));
        }

        if (devicesWithEvents > 0) {
            mpLogger->infof("[EVENT_HANDLER] [LOAD_DEVICES_EVENTS] Loaded events from %zu %s",
                            devicesWithEvents,
                            devicesWithEvents == 1 ? "device" : "devices");
        } else {
            mpLogger->info("[EVENT_HANDLER] [LOAD_DEVICES_EVENTS] No events found");
        }
    }

    void EventHandler::loadModulesNotifications() {
        mpLogger->debug("[EVENT_HANDLER] [LOAD_MODULES_NOTIFICATIONS] Called");
        std::unique_lock lock(mMutex);

        mModuleNotifications.clear();
        mLogicAddressToModuleId.clear();

        const auto cachedModules = Core::Instance().configCache().getAllModules();
        if (cachedModules.empty()) {
            mpLogger->warning("[EVENT_HANDLER] [LOAD_MODULES_NOTIFICATIONS] No cached modules found");
            return;
        }

        uint modulesWithNotifications = 0;
        for (const auto &module: cachedModules) {
            mLogicAddressToModuleId.emplace(module.logicAddress, module.id);

            if (!module.config.contains(cmck::ON_NOTIFICATION)) continue; // Ignore modules without notifications
            if (!module.config.at(cmck::ON_NOTIFICATION).is_object()) {
                mpLogger->errorf("[EVENT_HANDLER] [LOAD_MODULES_NOTIFICATIONS] Module [%u] has invalid "
                                 "'on_notifications' field in config, it must be an object", module.id);
                continue;
            }

            const auto &onNotification = module.config.at(cmck::ON_NOTIFICATION);

            // Type of notifications (e.g. 'alert'): notifications' objects
            std::unordered_map<std::string, std::vector<Notification> > notificationsOfType;

            for (const auto &[notificationsType, notifications]: onNotification.items()) {
                auto moduleNotificationsOfType = parseModuleNotifications(module.id, notifications);

                if (moduleNotificationsOfType.empty()) continue;

                notificationsOfType.emplace(notificationsType, std::move(moduleNotificationsOfType));
            }

            if (notificationsOfType.empty()) continue;

            modulesWithNotifications++;
            mModuleNotifications.emplace(module.id, std::move(notificationsOfType));
        }

        if (modulesWithNotifications > 0) {
            mpLogger->infof("[EVENT_HANDLER] Loaded notification actions for %u %s",
                            modulesWithNotifications,
                            modulesWithNotifications == 1 ? "module" : "modules");
        } else {
            mpLogger->debug("[EVENT_HANDLER] [LOAD_MODULES_NOTIFICATIONS] No module notification actions loaded");
        }
    }

    void EventHandler::handleEvents(const uint deviceId) {
        mpLogger->debug("[EVENT_HANDLER] [HANDLE_EVENTS] Called");
        if (!mIsRunning) return;
        std::shared_lock lock(mMutex);

        const auto iter = mDeviceEvents.find(deviceId);
        if (iter == mDeviceEvents.end()) return;

        auto &events = iter->second;

        const auto cachedReading = Core::Instance().readingsCache().get(deviceId);
        if (!cachedReading) {
            mpLogger->error("[EVENT_HANDLER] [HANDLE_EVENTS] Could not find device reading in cache");
            return;
        }
        const auto readingValue = cachedReading.value().value;

        const auto cachedDeviceConfig = Core::Instance().configCache().getDevice(deviceId);
        if (!cachedDeviceConfig) {
            mpLogger->error("[EVENT_HANDLER] [HANDLE_EVENTS] Could not find device config in cache");
            return;
        }
        const auto deviceConfig = cachedDeviceConfig.value().config;

        if (!deviceConfig.contains(cdck::VALUES) ||
            !deviceConfig.at(cdck::VALUES).is_array() ||
            deviceConfig.at(cdck::VALUES).empty()) {
            mpLogger->error("[EVENT_HANDLER] [HANDLE_EVENTS] invalid '"s + cdck::VALUES.data()
                            + "' device config field, it should contain expected reading values in an array");
            return;
        }

        const auto &deviceExpectedValuesFormat = deviceConfig.at(cdck::VALUES);

        // FIXME !pr notif is send when condition is invalid (works properly when condition is not met)
        for (auto &event: events) {
            if (shouldEventTrigger(event, readingValue, deviceExpectedValuesFormat))
                dispatchAction(deviceId, event.action);;
        }
    }

    void EventHandler::handleNotification(const uint logicAddress, const std::string_view notificationType) {
        if (!mIsRunning) return;
        mpLogger->debugf("[EVENT_HANDLER] [HANDLE_NOTIFICATION] type='%s' logicAddress=%u",
                         notificationType.data(), logicAddress);
        std::shared_lock lock(mMutex);

        // Find module ID
        const auto logicAddressIt = mLogicAddressToModuleId.find(logicAddress);
        if (logicAddressIt == mLogicAddressToModuleId.end()) {
            mpLogger->warningf(
                "[EVENT_HANDLER] [HANDLE_NOTIFICATION] No module found for logicAddress=%u", logicAddress);
            return;
        }
        const auto moduleId = logicAddressIt->second;

        // Find all notifications of module
        const auto moduleNotificationsIt = mModuleNotifications.find(moduleId);
        if (moduleNotificationsIt == mModuleNotifications.end()) return;
        const auto &moduleNotifications = moduleNotificationsIt->second;

        // Find notifications of type matching "notificationType"
        const auto notificationsOfTypeIt = moduleNotifications.find(std::string(notificationType));
        if (notificationsOfTypeIt == moduleNotifications.end()) return;
        const auto &moduleNotificationsOfType = notificationsOfTypeIt->second;

        // Dispatch enabled notifications
        for (const auto &notification: moduleNotificationsOfType) {
            if (notification.enabled) dispatchModuleAction(moduleId, notification.action);
        }
    }

    void EventHandler::start() {
        if (mIsRunning.exchange(true, std::memory_order_acq_rel)) return;

        mpLogger->info("[EVENT_HANDLER] Starting event handler");
    }

    void EventHandler::stop() {
        if (mIsStopping.exchange(true, std::memory_order_acq_rel)) return;
        if (!mIsRunning.exchange(false, std::memory_order_acq_rel)) return;

        mpLogger->info("[EVENT_HANDLER] Stopping event handler");
    }

    EventHandler::Event::Event(const uint eventDeviceId, const nlohmann::json &event) {
        deviceId = eventDeviceId;

        if (!event.contains(cdck::ENABLED) ||
            !event.at(cdck::ENABLED).is_boolean()) {
            throw std::invalid_argument("Event must have boolean 'enabled' field");
        }

        enabled = event.at(cdck::ENABLED).get<bool>();

        if (!event.contains(cdck::TRIGGER) ||
            !event.at(cdck::TRIGGER).is_string()) {
            throw std::invalid_argument("Event must have string 'trigger' field");
        }

        const auto &triggerStr = event.at(cdck::TRIGGER).get<std::string>();

        if (!cdck::TRIGGER_TYPES.contains(triggerStr)) {
            std::string acceptedTriggerTypes;
            for (bool first = true; const auto &type: cdck::TRIGGER_TYPES) {
                if (!first) acceptedTriggerTypes.append(", ");
                acceptedTriggerTypes.append(type);
                first = false;
            }

            throw std::invalid_argument("Invalid 'trigger' field value, accepted values: " + acceptedTriggerTypes);
        }

        if (triggerStr == cdck::EDGE) trigger = TriggerType::EDGE;
        else if (triggerStr == cdck::LEVEL) trigger = TriggerType::LEVEL;

        if (!event.contains(cdck::CONDITION) ||
            !event.at(cdck::CONDITION).is_object()) {
            throw std::invalid_argument("Event must have object 'condition' field");
        }

        condition = event.at(cdck::CONDITION).get<nlohmann::json>();

        if (!event.contains(cdck::ACTION) ||
            !event.at(cdck::ACTION).is_object()) {
            throw std::invalid_argument("Event must have object 'action' field");
        }

        action = event.at(cdck::ACTION).get<nlohmann::json>();
    }

    EventHandler::Notification::Notification(const nlohmann::json &notification) {
        if (!notification.contains(cdck::ENABLED) ||
            !notification.at(cdck::ENABLED).is_boolean()) {
            throw std::invalid_argument("Notification must have boolean 'enabled' field");
        }

        enabled = notification.at(cdck::ENABLED).get<bool>();

        if (!notification.contains(cdck::ACTION) ||
            !notification.at(cdck::ACTION).is_object()) {
            throw std::invalid_argument("Notification must have object 'action' field");
        }

        action = notification.at(cdck::ACTION).get<nlohmann::json>();
    }

    std::vector<EventHandler::Event>
    EventHandler::parseDeviceEvents(const uint deviceId, const nlohmann::json &events) const {
        if (!events.is_array()) {
            mpLogger->errorf("[EVENT_HANDLER] Device [%zu] events parsing failed: 'events' field must be an array",
                             deviceId);
            return {};
        }

        if (events.empty()) return {};

        std::vector<Event> parsedEvents;
        parsedEvents.reserve(events.size());

        for (const auto &event: events) {
            try {
                parsedEvents.emplace_back(deviceId, event);
            } catch (const std::exception &e) {
                mpLogger->errorf("[EVENT_HANDLER] Device [%zu] event parsing failed: %s", deviceId, e.what());
            }
        }

        mpLogger->debugf("[EVENT_HANDLER] Device [%zu] events parsed successfully: %zu/%zu",
                         deviceId,
                         parsedEvents.size(),
                         events.size());
        return parsedEvents;
    }

    std::vector<EventHandler::Notification> EventHandler::parseModuleNotifications(const uint moduleId,
        const nlohmann::json &notifications) const {
        if (!notifications.is_array()) {
            mpLogger->errorf("[EVENT_HANDLER] Module [%zu] notifications parsing failed: "
                             "[notification_type] field must be an array",
                             moduleId);
            return {};
        }

        if (notifications.empty()) return {};

        std::vector<Notification> parsedNotifications;
        parsedNotifications.reserve(notifications.size());

        for (const auto &notification: notifications) {
            try { parsedNotifications.emplace_back(notification); } catch (const std::exception &e) {
                mpLogger->errorf("[EVENT_HANDLER] Module [%zu] notification parsing failed: %s", moduleId, e.what());
            }
        }

        mpLogger->debugf("[EVENT_HANDLER] Device [%zu] events parsed successfully: %zu/%zu",
                         moduleId,
                         parsedNotifications.size(),
                         notifications.size());

        return parsedNotifications;
    }

    bool EventHandler::shouldEventTrigger(Event &event,
                                          const nlohmann::json &reading,
                                          const nlohmann::json::array_t &deviceExpectedValuesFormat) const {
        const bool isReadingAnArray = reading.is_array();

        if (isReadingAnArray && reading.size() != deviceExpectedValuesFormat.size()) {
            mpLogger->error("[EVENT_HANDLER] Reading is not in expected format, check device config 'values' field");
            return false;
        }

        auto condition = event.condition;
        bool result = false;

        if (isReadingAnArray) {
            // Handle array readings with possible multiple conditions (all conditions must be true)
            uint index = 0;
            result = true; // inverse logic with for loop
            bool containsValidCondition = false; // used to check if at least one condition is valid

            for (const auto &readingRawValue: reading) {
                const auto valueConditionKey = "$" + std::to_string(index);
                index++; // TODO !pr check if correct

                // Check for condition
                if (!condition.contains(valueConditionKey)) continue; // No condition for value
                if (!condition.at(valueConditionKey).is_object()) {
                    mpLogger->errorf("[EVENT_HANDLER] Condition must be an object, check device [%zu] config",
                                     event.deviceId);
                    continue;
                }
                containsValidCondition = true;

                // Check formatting
                auto readingFormattedValue = readingRawValue;
                const auto &valueFormatting = deviceExpectedValuesFormat[index];

                if (valueFormatting.contains(cdck::PRECISION) &&
                    readingRawValue.is_number_float()) {
                    if (valueFormatting.at(cdck::PRECISION).is_number_integer()) {
                        readingFormattedValue = std::round(readingRawValue.get<double>());
                    } else {
                        mpLogger->errorf("[EVENT_HANDLER] Error while formating reading: "
                                         "'precision' field value must be an integer, check device [%zu] config",
                                         event.deviceId);
                    }
                }

                // Check if condition is met
                if (!isConditionMet(readingFormattedValue, condition.at(valueConditionKey))) {
                    result = false;
                    break;
                }
                // Delete handled conditions to check later for invalid ones
                condition.erase(valueConditionKey);
            }

            if (result && !containsValidCondition) {
                result = false;
                mpLogger->warningf(
                    "[EVENT_HANDLER] Event does not contain any valid conditions, check device [%zu] config",
                    event.deviceId);
            }

            if (result && !condition.empty()) {
                mpLogger->warningf(
                    "[EVENT_HANDLER] Invalid conditions not handled, check device [%zu] config. JSON dump: %s",
                    event.deviceId, condition.dump().c_str());
            }
        } else if (condition.contains("$0") && condition.at("$0").is_object()) {
            // Handle single value reading with condition in '$0' field in conditions object
            result = isConditionMet(reading, condition.at("$0"));
        } else {
            // Handle single value reading with condition directly in conditions object
            result = isConditionMet(reading, condition);
        }


        if (result) {
            // Ignore continuous triggers if trigger is set to edge
            if (event.trigger == TriggerType::EDGE && event.triggered) {
                mpLogger->debugf("[EVENT_HANDLER] Event with edge ignored for device [%zu]", event.deviceId);
                return false;
            }
            event.triggered = true;
        } else {
            // Reset triggered state when condition is not met
            event.triggered = false;
        }

        return result;
    }

    bool EventHandler::isConditionMet(const nlohmann::json &value, const nlohmann::json &condition) const {
        if (!value.is_number() && !value.is_string()) {
            mpLogger->error("[EVENT_HANDLER] Unsupported value type for condition evaluation");
            return false;
        }

        if (value.is_number()) return compareNumeric(value, condition);
        if (value.is_string()) return compareString(value, condition);

        mpLogger->error("[EVENT_HANDLER] Unexpected condition");
        return false;
    }

    bool EventHandler::compareNumeric(const nlohmann::json &value, const nlohmann::json &condition) const {
        static const std::unordered_map<std::string, std::function<bool(double, double)> > numericalOperators = {
            {">", [](const double a, const double b) { return a > b; }},
            {"<", [](const double a, const double b) { return a < b; }},
            {"=", [](const double a, const double b) { return a == b; }},
            {"!=", [](const double a, const double b) { return a != b; }},
            {">=", [](const double a, const double b) { return a >= b; }},
            {"<=", [](const double a, const double b) { return a <= b; }},
        };

        const double numValue = value.get<double>();

        for (const auto &[op, threshold]: condition.items()) {
            if (!threshold.is_number()) {
                mpLogger->errorf("[EVENT_HANDLER] Threshold for operator '%s' must be a number", op.c_str());
                return false;
            }
            const auto iter = numericalOperators.find(op);
            if (iter == numericalOperators.end()) {
                mpLogger->errorf("[EVENT_HANDLER] Unknown operator '%s'", op.c_str());
                return false;
            }

            mpLogger->debugf("[EVENT_HANDLER] [TEST] %s %s %s = %b", to_string(value).c_str(), op.c_str(),
                             to_string(threshold).c_str(), iter->second(numValue, threshold.get<double>()));

            return iter->second(numValue, threshold.get<double>()); // TODO !pr check if correct
        }

        mpLogger->debug("[EVENT_HANDLER] No operators found in condition");
        return false;
    }

    bool EventHandler::compareString(const nlohmann::json &value, const nlohmann::json &condition) const {
        static const std::unordered_map<std::string, std::function<bool(const std::string &, const std::string &)> >
                stringOperators = {
                    {"=", [](const std::string &a, const std::string &b) { return a == b; }},
                    {"!=", [](const std::string &a, const std::string &b) { return a != b; }},
                    {"contains", [](const std::string &a, const std::string &b) { return a.contains(b); }}
                };

        const auto stringValue = value.get<std::string>();

        for (const auto &[op, target]: condition.items()) {
            if (!target.is_string()) {
                mpLogger->errorf("[EVENT_HANDLER] Target for operator '%s' must be a string", op.c_str());
                return false;
            }
            const auto iter = stringOperators.find(op);
            if (iter == stringOperators.end()) {
                mpLogger->errorf("[EVENT_HANDLER] Unknown operator '%s'", op.c_str());
                return false;
            }

            mpLogger->debugf("[EVENT_HANDLER] [TEST] %s %s %s = %b", to_string(value).c_str(), op.c_str(),
                             to_string(target).c_str(), iter->second(stringValue, target.get<std::string>()));

            return iter->second(stringValue, target.get<std::string>()); // TODO !pr check if correct
        }

        mpLogger->debug("[EVENT_HANDLER] No operators found in condition");
        return false;
    }

    void EventHandler::dispatchAction(uint deviceId, const nlohmann::json &action) const {
        if (!mIsRunning) return;
        mpLogger->debug("[EVENT_HANDLER] Dispatching action");

        boost::asio::post(mIoContext, [action = action, deviceId, this] {
            if (!mIsRunning) return;
            const std::string_view actionName = "Conditional event";
            ActionHelpers::dispatchAutomatedDeviceAction(actionName, deviceId, action);
        });
    }

    void EventHandler::dispatchModuleAction(const uint moduleId, const nlohmann::json &action) const {
        if (!mIsRunning) return;
        mpLogger->debugf("[EVENT_HANDLER] Dispatching module notification action for module [%u]", moduleId);

        boost::asio::post(mIoContext, [action = action, moduleId, this] {
            if (!mIsRunning) return;
            const std::string_view actionName = "Module notification";
            ActionHelpers::dispatchAutomatedModuleAction(actionName, moduleId, action);
        });
    }
}
