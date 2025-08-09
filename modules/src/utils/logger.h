#pragma once

#include <Arduino.h>

#include "smart_home_config.h"

namespace Utils {
    namespace Logging {
        /**
         * @brief Enumeration of available log levels.
         */
        enum class Level : uint8_t {
            NONE = 0, ///< Prints nothing
            ERROR = 1, ///< Prints only errors
            WARNING = 2, ///< Prints warnings and errors
            INFO = 3, ///< Prints information, warnings and errors
            DEBUG = 4, ///< Prints everything
        };
        static constexpr auto minLevel = Level::ERROR;
        static constexpr auto maxLevel = Level::DEBUG;

        // TODO remove?
        /**
         * @brief Convert log level to string representation.
         *
         * @param level log level to convert.
         * @return String view of the level name.
         */
        // static constexpr std::string_view toString(const Level level) {
        //     switch (level) {
        //         case Level::NONE: return "NONE";
        //         case Level::ERROR: return "ERROR";
        //         case Level::WARNING: return "WARNING";
        //         case Level::INFO: return "INFO";
        //         case Level::DEBUG: return "DEBUG";
        //         default: return "UNDEFINED";
        //     }

        class Logger {
        public:
            explicit Logger(Level level = static_cast<Level>(DEFAULT_LOGGING_LEVEL));
            ~Logger();

            void error(const char *name, const char *message);
            void warning(const char *name, const char *message);
            void info(const char *name, const char *message);
            void debug(const char *name, const char *message);
        private:
            void writeLog(Level level, const char *name, const char *functionName, const char *message);

            Level mLogLevel;
        };
    }
}