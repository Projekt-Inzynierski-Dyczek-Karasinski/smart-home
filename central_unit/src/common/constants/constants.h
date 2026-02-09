#pragma once
#include <c++/12/string_view>

namespace SmartHome::Constants {
    namespace Common {
        inline constexpr std::string_view UNDEFINED = "undefined";
        inline constexpr std::string_view UNKNOWN = "unknown";

        // Special
        inline constexpr std::string_view UNDEFINED_BRACKETS = "<undefined>";
        inline constexpr std::string_view NONE_BRACKETS = "<none>";
        inline constexpr std::string_view DEFAULT_UPPER = "DEFAULT";
        inline constexpr std::string_view CRLF = "\r\n";
    }

    namespace DefaultPaths {
        inline constexpr std::string_view UDS = "/var/run/smarthomed.sock";

        inline constexpr std::string_view MEDIATOR_CONFIG = "/etc/smarthome/mediator.yaml";
        inline constexpr std::string_view MEDIATOR_LOGFILE = "/var/log/smarthome/mediator.log";
        inline constexpr std::string_view MEDIATOR_EXEC = "/usr/local/bin/smarthome-mediator";

        inline constexpr std::string_view DB_SERVICE_CONFIG = "/etc/smarthome/db-service.yaml";
        inline constexpr std::string_view DB_SERVICE_LOGFILE = "/var/log/smarthome/db-service.log";
        inline constexpr std::string_view DB_SERVICE_EXEC = "/usr/local/bin/smarthome-database";

        inline constexpr std::string_view CORE_CONFIG = "/etc/smarthome/smart_home.yaml";
        inline constexpr std::string_view CORE_LOGFILE = "/var/log/smarthome/core.log";
    }

    namespace DefaultServiceNames {
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
        inline constexpr std::string_view MODULE_SENSORS = "module_sensors";
        inline constexpr std::string_view SENSORS = "sensors";
        inline constexpr std::string_view SENSOR = "sensor";

        // Historical data endpoints (get only, sent to database)
        inline constexpr std::string_view SENSOR_READINGS = "sensor_readings";
        inline constexpr std::string_view LOGS = "logs";

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
        inline constexpr std::string_view SENSOR_LIST = "sensor_list";
        inline constexpr std::string_view SENSOR_VALUE = "sensor_value";
        inline constexpr std::string_view ACTUATOR_VALUE = "actuator_value";
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
        inline constexpr std::string_view SENSORS_CHANGED = "sensors_changed";
    }

    namespace MediatorSpecial {
        inline constexpr std::string_view ALL_OPTIONS = "all_options";
        inline constexpr std::string_view CHANNEL = "channel";
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
}
