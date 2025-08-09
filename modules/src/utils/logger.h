#pragma once

#include <Arduino.h>

#include "smart_home_config.h"

namespace Utils {
    namespace Logging {
        /**
         * @brief Enumeration of available log levels.
         */
        enum class Level : uint8_t {
            NONE = 0, ///< Prints nothing (for production)
            ERROR = 1, ///< Prints only errors
            WARNING = 2, ///< Prints warnings and errors
            INFO = 3, ///< Prints information, warnings and errors
            DEBUG = 4, ///< Prints everything
        };

        class Logger {
        public:
            explicit Logger(Level level = static_cast<Level>(LOGGING_LEVEL));
            ~Logger() = default;

            /**
             * @brief Logs an <b>error</b> message.
             * @param name Name of the location of the log.
             * @param message Message of the log.
             */
            void error(const char *name, const char *message);
            /**
             * @brief Logs a <b>warning</b> message.
             * @param name Name of the location of the log.
             * @param message Message of the log.
             */
            void warning(const char *name, const char *message);
            /**
             * @brief Logs an <b>info</b> message.
             * @param name Name of the location of the log.
             * @param message Message of the log.
             */
            void info(const char *name, const char *message);
            /**
             * @brief Logs a <b>debug</b> message.
             * @param name Name of the location of the log.
             * @param message Message of the log.
             */
            void debug(const char *name, const char *message);
        private:
            /**
             * @brief Convert log level to char array representation.
             * @param buffer Array to store name of log level.
             * @param level Log level to convert.
             * @return True if successfully converted log level, false otherwise.
             */
            bool logLevelToString(char *buffer, Level level);


            void writeLog(Level level, const char *name, const char *message);

            Level mLogLevel; ///< Currently set logging level.
        };
    }
}