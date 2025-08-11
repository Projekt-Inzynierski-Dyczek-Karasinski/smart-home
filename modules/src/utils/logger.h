#pragma once

#include <Arduino.h>

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

            Level getLogLevel() const;

            /**
             * @brief Logs an <i>error</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void error(const char *name, const char *message);
            /**
             * @brief Logs an <i>error</i> message with given integer value.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void errorv(const char *name, const char *message, int value);
            /**
             * @brief Logs an <i>error</i> message with given uint8_t array.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param values Array of values to add to end of log.
             * @param len Length of array.
             * @param isAscii True if values in array should be converted to chars before print, false otherwise,\n default: true
             */
            void errora(const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii = true);

            /**
             * @brief Logs a <i>warning</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void warning(const char *name, const char *message);
            /**
             * @brief Logs a <i>warning</i> message with given integer value.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void warningv(const char *name, const char *message, int value);
            /**
             * @brief Logs a <i>warning</i> message with given uint8_t array.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param values Array of values to add to end of log.
             * @param len Length of array.
             * @param isAscii True if values in array should be converted to chars before print, false otherwise,\n default: true
             */
            void warninga(const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii = true);

            /**
             * @brief Logs an <i>info</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void info(const char *name, const char *message);
            /**
             * @brief Logs an <i>info</i> message with given integer value.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void infov(const char *name, const char *message, int value);
            /**
             * @brief Logs an <i>info</i> message with given uint8_t array.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param values Array of values to add to end of log.
             * @param len Length of array.
             * @param isAscii True if values in array should be converted to chars before print, false otherwise,\n default: true
             */
            void infoa(const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii = true);

            /**
             * @brief Logs a <i>debug</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void debug(const char *name, const char *message);
            /**
             * @brief Logs a <i>debug</i> message with given integer value.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void debugv(const char *name, const char *message, int value);
            /**
             * @brief Logs a <i>debug</i> message with given uint8_t array.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param values Array of values to add to end of log.
             * @param len Length of array.
             * @param isAscii True if values in array should be converted to chars before print, false otherwise,\n default: true
             */
            void debuga(const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii = true);

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
             * @brief Prepares a log and calls <code>writeLog()</code> to write it if level <= logging level.
             * @param level Log severity level of the message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void log(Level level, const char *name, const char *message);
            /**
             * @brief Prepares a log and calls <code>writeLog()</code> to write it if level <= logging level.
             * @param level Log severity level of the message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void log(Level level, const char *name, const char *message, int value);
            /**
             * @brief Prepares a log and calls <code>writeLog()</code> to write it if level <= logging level.
             * @param level Log severity level of the message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param values Array of values to add to end of log.
             * @param len Length of array.
             * @param isAscii True if values in array should be converted to chars before print, false otherwise.
             */
            void log(Level level, const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii);

            /**
             * @brief Writes a log.
             * @param logType String representation of log level.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param newLine True if log should end with new line, false otherwise,\n default: true.
             */
            void writeLog(const char *logType, const char *name, const char *message, bool newLine = true) const;

            Level mLogLevel; ///< Currently set logging level.

            static xSemaphoreHandle smSerialBeginMutex; ///< Static handle to FreeRTOS mutex protecting <code>smIsSerialBegin</code>
            static bool smIsSerialBegin; ///< Static flag ensuring that <code>Serial.begin()</code> is called only once.
        };
    }
}