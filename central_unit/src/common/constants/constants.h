#pragma once
#include <c++/12/string_view>
#include <set>

namespace SmartHome::Constants {
    namespace Common {
        inline constexpr std::string_view UNDEFINED = "undefined";
        inline constexpr std::string_view UNKNOWN = "unknown";
        inline constexpr std::string_view TYPE = "type";
        inline constexpr std::string_view OK = "ok";

        // Special
        inline constexpr std::string_view UNDEFINED_BRACKETS = "<undefined>";
        inline constexpr std::string_view NONE_BRACKETS = "<none>";
        inline constexpr std::string_view DEFAULT_UPPER = "DEFAULT";
    }

    namespace DefaultPaths {
        inline constexpr std::string_view UDS = "/var/run/smarthomed.sock";
        inline constexpr std::string_view WEB_ROOT = "/var/www/smarthome";

        inline constexpr std::string_view MEDIATOR_CONFIG = "/etc/smarthome/mediator.yaml";
        inline constexpr std::string_view MEDIATOR_LOGFILE = "/var/log/smarthome/mediator.log";
        inline constexpr std::string_view MEDIATOR_EXEC = "/usr/local/bin/smarthome-mediator";

        inline constexpr std::string_view DB_SERVICE_CONFIG = "/etc/smarthome/db-service.yaml";
        inline constexpr std::string_view DB_SERVICE_LOGFILE = "/var/log/smarthome/db-service.log";
        inline constexpr std::string_view DB_SERVICE_EXEC = "/usr/local/bin/smarthome-database";

        inline constexpr std::string_view WEB_SERVER_CONFIG = "/etc/smarthome/web-server.yaml";
        inline constexpr std::string_view WEB_SERVER_LOGFILE = "/var/log/smarthome/web-server.log";

        inline constexpr std::string_view CORE_CONFIG = "/etc/smarthome/smart_home.yaml";
        inline constexpr std::string_view CORE_LOGFILE = "/var/log/smarthome/core.log";
    }

    namespace DefaultServiceNames {
        inline constexpr std::string_view WEB = "smarthome-webd";
        inline constexpr std::string_view MEDIATOR = "smarthome-radiod";
        inline constexpr std::string_view DATABASE = "smarthome-databased";
        inline constexpr std::string_view CORE = "smarthomed";
    }

    namespace Methods {
        inline constexpr std::string_view GET = "get";
        inline constexpr std::string_view SET = "set";
        inline constexpr std::string_view NOTIFY = "notify";
        inline constexpr std::string_view EXECUTE = "execute";
        inline constexpr std::string_view PING = "ping";
        /// \c ECHO is a macro, \c ECHO_STR is used to avoid conflicts
        inline constexpr std::string_view ECHO_STR = "echo";
        inline constexpr std::string_view DELETE = "delete";
    }

    /// Used with set methods
    namespace OperationModes {
        inline constexpr std::string_view OVERWRITE = "overwrite";
        inline constexpr std::string_view APPEND = "append";

        inline const std::set MODES = {
            OVERWRITE, APPEND
        };
    }

    namespace Targets {
        inline constexpr std::string_view MODULE_MEDIATOR = "module_mediator";
        inline constexpr std::string_view MEDIATOR = "mediator";
        inline constexpr std::string_view CORE = "core";
        inline constexpr std::string_view DATABASE = "database";
        inline constexpr std::string_view CLI = "cli";
        inline constexpr std::string_view GUI = "gui";
        inline constexpr std::string_view WEB_SERVER = "web_server";
        inline constexpr std::string_view WEB = "web";
    }

    namespace CoreTypes {
        // Config / metadata endpoints (get only, read from cache)
        inline constexpr std::string_view MODULES = "modules";
        inline constexpr std::string_view MODULE = "module";
        inline constexpr std::string_view MODULE_DEVICES = "module_devices";
        inline constexpr std::string_view DEVICES = "devices";
        inline constexpr std::string_view DEVICE = "device";

        // Historical data endpoints (get only, sent to database)
        inline constexpr std::string_view DEVICE_READINGS = "device_readings";
        inline constexpr std::string_view LOGS = "logs";

        // Current data endpoints (sent to mediator)
        inline constexpr std::string_view DEVICE_VALUE = "device_value";

        // Core config keys (set only)
        inline constexpr std::string_view CONNECTION_TYPE = "connection_type";
        // Core keys (get only)
        inline constexpr std::string_view _DEBUG_CACHE = "_debug_cache";
    }

    namespace MediatorTypes {
        // Set/Get type strings
        inline constexpr std::string_view CONFIG_OPTION = "config_option";

        // Set type strings
        inline constexpr std::string_view TOGGLE_ACTUATOR = "toggle_actuator";
        inline constexpr std::string_view SET_ACTUATOR_VALUE = "set_actuator_value";

        // Get type strings
        inline constexpr std::string_view DEVICE_LIST = "device_list";
        inline constexpr std::string_view SENSOR_VALUE = "sensor_value";
        inline constexpr std::string_view ACTUATOR_STATE = "actuator_state";
        inline constexpr std::string_view FORCE_READ_SENSOR_VALUE = "force_read_sensor_value";
        inline constexpr std::string_view BATTERY_LEVEL = "battery_level";
        inline constexpr std::string_view MODULE_LOGS = "module_logs";

        // Notify types strings
        // Sent to modules
        inline constexpr std::string_view MANUAL_TRIGGER = "manual_trigger";
        inline constexpr std::string_view POWER_LOSS = "power_loss";
        inline constexpr std::string_view ALERT = "alert";
        // Sent to core
        inline constexpr std::string_view WAKE = "wake";

        // Execute types strings
        inline constexpr std::string_view SLEEP = "sleep";
        inline constexpr std::string_view DEEP_SLEEP = "deep_sleep";
    }

    namespace DatabaseTypes {
        // Db notifications
        inline constexpr std::string_view MODULES_CHANGED = "modules_changed";
        inline constexpr std::string_view DEVICES_CHANGED = "devices_changed";
    }

    namespace MediatorSpecial {
        inline constexpr std::string_view ALL_OPTIONS = "all_options";
        inline constexpr std::string_view CHANNEL = "channel";
    }

    namespace DeviceTypes {
        inline constexpr std::string_view SENSOR = "sensor";
        inline constexpr std::string_view ACTUATOR = "actuator";

        inline const std::set TYPES = {
            SENSOR, ACTUATOR
        };
    }

    /// Some identifiers in this namespace intentionally duplicate constants from other namespaces.
    /// They are duplicated to separate the database schema constants from the application constants.
    namespace DatabaseIdentifiers {
        // Tables
        inline constexpr std::string_view MODULES = "modules";
        inline constexpr std::string_view DEVICES = "devices";
        inline constexpr std::string_view DEVICE_READINGS = "device_readings";
        inline constexpr std::string_view LOGS = "logs";

        inline const std::set TABLES = {
            MODULES, DEVICES, DEVICE_READINGS, LOGS
        };

        // Shared table columns
        inline constexpr std::string_view ID = "id";
        inline constexpr std::string_view CONFIG = "config";
        inline constexpr std::string_view CREATED_AT = "created_at";
        inline constexpr std::string_view NAME = "name";
        inline constexpr std::string_view MODULE_ID = "module_id";
        inline constexpr std::string_view TYPE = "type";
        inline constexpr std::string_view TIMESTAMP = "timestamp";

        // Modules table columns (unique)
        inline constexpr std::string_view LOGIC_ADDRESS = "logic_address";
        inline constexpr std::string_view LAST_ONLINE = "last_online";

        inline const std::set MODULES_TABLE_COLUMNS = {
            ID, LOGIC_ADDRESS, NAME, CONFIG, LAST_ONLINE, CREATED_AT
        };

        // Devices table columns (unique)
        inline constexpr std::string_view LOGIC_ID = "logic_id";

        inline const std::set DEVICES_TABLE_COLUMNS = {
            ID, LOGIC_ID, NAME, TYPE, CONFIG, CREATED_AT, MODULE_ID
        };

        // Device readings table columns (unique)
        inline constexpr std::string_view VALUE_TEXT = "value_text";
        inline constexpr std::string_view VALUE_NUMERIC = "value_numeric";
        inline constexpr std::string_view METADATA = "metadata";
        inline constexpr std::string_view DEVICE_ID = "device_id";

        inline const std::set DEVICE_READINGS_TABLE_COLUMNS = {
            ID, TIMESTAMP, VALUE_TEXT, VALUE_NUMERIC, METADATA, DEVICE_ID
        };

        // Logs table columns (unique)
        inline constexpr std::string_view CONTENT = "content";

        inline const std::set LOGS_TABLE_COLUMNS = {
            ID, TYPE, CONTENT, TIMESTAMP, MODULE_ID
        };

        inline const std::set ALL_IDENTIFIERS = [] {
            std::set<std::string_view> set;
            set.insert(TABLES.begin(), TABLES.end());
            set.insert(MODULES_TABLE_COLUMNS.begin(), MODULES_TABLE_COLUMNS.end());
            set.insert(DEVICES_TABLE_COLUMNS.begin(), DEVICES_TABLE_COLUMNS.end());
            set.insert(DEVICE_READINGS_TABLE_COLUMNS.begin(), DEVICE_READINGS_TABLE_COLUMNS.end());
            set.insert(LOGS_TABLE_COLUMNS.begin(), LOGS_TABLE_COLUMNS.end());
            return set;
        }();
    }

    namespace DeviceConfigKeys {
        inline constexpr std::string_view USE_CACHE = "use_cache";
        inline constexpr std::string_view CACHE_TTL = "cache_ttl";

        inline constexpr std::string_view VALUES = "values";
        // Value keys
        inline constexpr std::string_view INDEX = "index";
        inline constexpr std::string_view LABEL = "label";
        inline constexpr std::string_view UNIT = "unit";
        inline constexpr std::string_view PRECISION = "precision";
        // -----------------

        inline constexpr std::string_view EVENTS = "events";
        // Event keys
        inline constexpr std::string_view CONDITION = "condition";
        // -----------------

        inline constexpr std::string_view SCHEDULE = "schedule";
        // Schedule keys
        inline constexpr std::string_view RRULE = "rrule";
        inline constexpr std::string_view DTSTART = "dtstart";
        // -----------------

        // Events / Schedule keys
        inline constexpr std::string_view ENABLED = "enabled";
        inline constexpr std::string_view ACTION = "action";
        // Action keys - shared with SmartHome::JsonRpcStrings

        inline const std::set CONFIG_KEYS = {
            USE_CACHE, CACHE_TTL, VALUES, INDEX, LABEL, UNIT, PRECISION, EVENTS, CONDITION, SCHEDULE, RRULE, DTSTART,
            ENABLED, ACTION
        };
    }

    namespace ModuleConfigKeys {
        inline constexpr std::string_view CONNECTION = "connection";
        inline constexpr std::string_view RF_CHANNEL = "rf_channel";

        inline constexpr std::string_view SLEEP_AFTER_SEND = "sleep_after_send";
        inline constexpr std::string_view POWER_SAVING = "power_saving";
        inline constexpr std::string_view DEFAULT_SLEEP_DURATION = "default_sleep_duration";

        inline const std::set CONFIG_KEYS = {
            CONNECTION, RF_CHANNEL, SLEEP_AFTER_SEND, POWER_SAVING, DEFAULT_SLEEP_DURATION
        };
    }

    namespace HumanReadableErrors {
        inline constexpr std::string_view UNKNOWN_ERROR = "Unknown error";
        inline constexpr std::string_view BAD_COMMAND = "Bad command";
        inline constexpr std::string_view UNKNOWN_COMMAND = "Unknown command";
        inline constexpr std::string_view BAD_ARGUMENT = "Bad argument";
        inline constexpr std::string_view NOT_IMPLEMENTED = "Not implemented";
        inline constexpr std::string_view INTERNAL_ERROR = "Internal error";
        inline constexpr std::string_view UNDEFINED_ERROR = "Undefined error";
    }

    // Utility functions

    constexpr std::string_view methodTypeToDbTable(const std::string_view setType) {
        if (setType == CoreTypes::MODULE) return DatabaseIdentifiers::MODULES;
        if (setType == CoreTypes::DEVICE) return DatabaseIdentifiers::DEVICES;
        return "";
    }
}
