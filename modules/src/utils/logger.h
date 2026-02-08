#pragma once

#include <Arduino.h>
#include <atomic>

#include "../../config/system_config/logger_config.h"

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
            VERBOSE = 4, /// Prints everything except debug messages
            DEBUG = 5, ///< Prints everything
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
             * @warning Assumes that <code>Serial.begin()</code> is <b>not</b> called outside of this class. \n
             * Do <b>not</b> call <code>Serial.end()</code> due to issues with Serial on ESP32.
             * @note Thread-safe.
             */
            explicit Logger(Level level = static_cast<Level>(LOGGING_LEVEL));

            ~Logger() = default;

            /**
             * @brief Getter returning currently set log level.
             * @return Currently set log level.
             * @note Thread-safe.
             */
            Level getLogLevel() const;

            /**
             * @brief Setter for log level.
             * @param newLevel Level to set.
             * @note Thread-safe.
             */
            void setLogLevel(Level newLevel);

            /**
             * @brief Static getter returning if Serial is enabled (was called <code>Serial.begin()</code>).
             * @return True if Serial is enabled, false otherwise.
             * @note Thread-safe.
             */
            static bool getIsSerialEnabled();

            /**
             * @brief Waits for ongoing Serial transmission to complete and disables it.
             */
            static void waitAndDisable();

            /**
             * @brief Handles printing current input buffer (for terminal input task in Communication class).
             * @param letter Letter which will be printed.
             */
            void printInputChar(uint8_t letter);

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
             * @brief Logs a <i>verbose</i> message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             */
            void verbose(const char *name, const char *message);
            /**
             * @brief Logs a <i>verbose</i> message with given integer value.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             */
            void verbosev(const char *name, const char *message, int value);
            /**
             * @brief Logs a <i>verbose</i> message with given uint8_t array.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param values Array of values to add to end of log.
             * @param len Length of array.
             * @param isAscii True if values in array should be converted to chars before print, false otherwise,\n default: true
             */
            void verbosea(const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii = true);

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
             * @note Thread-safe.
             */
            void log(Level level, const char *name, const char *message);
            /**
             * @brief Prepares a log and calls <code>writeLog()</code> to write it if level <= logging level.
             * @param level Log severity level of the message.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param value Value to add to end of log.
             * @note Thread-safe.
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
             * @note Thread-safe.
             */
            void log(Level level, const char *name, const char *message, const uint8_t *values, uint8_t len, bool isAscii);

            /**
             * @brief Writes a log and reprints input buffer if needed.
             * @param logType String representation of log level.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param newLine True if log should end with new line, false otherwise,\n default: true.
             * @warning This method is <b>not</b> thread-safe by its own and should be called only in <code>log()</code> method.
             */
            void writeLog(const char *logType, const char *name, const char *message, bool newLine = true) const;

            static xSemaphoreHandle smSerialMutex; ///< Static handle to FreeRTOS mutex protecting changing settings of Serial and printing.
            static bool smIsSerialEnabled; ///< Static flag ensuring that <code>Serial.begin()</code> is called only once.

            Level mLogLevel = Level::NONE; ///< Currently set logging level.

            const size_t m_LOG_TYPE_LENGTH = 10;
            char mInputBuffer[MESSAGE_SIZE]{};
            uint8_t mInputBufferIndex = 0;

            static bool msIsLoggingDisabled; ///< Saved in RTC memory, to disable logging.
        };
    }
}