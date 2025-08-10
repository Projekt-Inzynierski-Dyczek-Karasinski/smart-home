#pragma once

#include <Arduino.h>

#include "smart_home_config.h"

namespace Utils {
    /**
     * @brief Namespace containing enum Level and class Logger.
     */
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

        /**
         * @brief Class responsible for logging (printing to the terminal) log messages with different verbosity levels.
         * @details The first instance of the Logger class will call <code>Serial.begin()</code> with the baudrate set in the config.
         * The class ensures that <code>Serial.begin()</code> is called only once (assuming that <code>Serial.begin()</code> is <b>not</b> called outside of this class).
         * If logging level is set to <code>Level::NONE</code>, Serial will not begin and the class will do nothing.
         * @note Thread-safe.
         */
        class Logger {
        public:
            /**
             * @brief Constructor to initialize Logger with a specific log level.
             * @details If the log level is NONE, logging is disabled.
             * The constructor will ensure Serial is initialized once across all instances.
             * @param level Logging level to set for this Logger instance,\n default: logging level set in the config.
             * @warning Assumes that <code>Serial.begin()</code> is <b>not</b> called outside of this class.
             * @note Thread-safe.
             */
            explicit Logger(Level level = static_cast<Level>(LOGGING_LEVEL));
            ~Logger() = default;

            /**
             * @brief Logs an <i>error</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void error(const char *name, const char *message);
            /**
             * @brief Logs an <i>error</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void errorWithValue(const char *name, const char *message, uint8_t value);
            /**
             * @brief Logs an <i>error</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Array of values to add to end of log.
             */
            void errorWithValue(const char *name, const char *message, const uint8_t *value);
            /**
             * @brief Logs a <i>warning</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void warning(const char *name, const char *message);
            /**
             * @brief Logs an <i>info</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void info(const char *name, const char *message);
            /**
             * @brief Logs a <i>debug</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void debug(const char *name, const char *message);

        private:
            /**
             * @brief Begins serial with baudrate set in config.\n
             * Ensures that <code>Serial.begin()</code> is called only once across all Logger instances.
             * @warning Assumes that <code>Serial.begin()</code> is <b>not</b> called outside of this class.
             * @note Thread-safe.
             */
            void beginSerial();

            /**
             * @brief Convert log level to char array representation.
             * @param buffer Array to store the name of the log level.
             * @param level Log level to convert.
             * @return True if successfully converted, false otherwise.
             */
            bool logLevelToString(char *buffer, Level level);

            /**
             * @brief Writes a log if the given log level is less than or equal to the currently set log level.
             * @param level Log severity level of the message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void writeLog(Level level, const char *name, const char *message);

            Level mLogLevel; ///< Currently set logging level.

            static xSemaphoreHandle smSerialBeginMutex; ///< Static handle to FreeRTOS mutex protecting <code>smIsSerialBegin</code>
            static bool smIsSerialBegin; ///< Static flag ensuring that <code>Serial.begin()</code> is called only once.
        };
    }
}