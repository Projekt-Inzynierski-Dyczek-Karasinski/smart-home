#pragma once

#include <Arduino.h>
#include <atomic>

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

            // TODO before merge with main remove commented code/rollback atomic
            // /**
            //  * @brief Cleans up FreeRTOS resources used by the class.
            //  */
            // ~Logger();
            ~Logger() = default;

            /**
             * @brief Getter returning currently set log level.
             * @return Currently set log level.
             * @note Thread-safe.
             */
            Level getLogLevel() const;

            /**
             * @brief Static getter returning if Serial is enabled (was called <code>Serial.begin()</code>).
             * @return True if Serial is enabled, false otherwise.
             * @note Thread-safe.
             */
            static bool getIsSerialEnabled();

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
             * @brief Writes a log.
             * @param logType String representation of log level.
             * @param name Name of the location/purpose of the log.
             * @param message The log message.
             * @param newLine True if log should end with new line, false otherwise,\n default: true.
             * @warning This method is <b>not</b> thread-safe by its own and should be called only in <code>log()</code> method.
             */
            void writeLog(const char *logType, const char *name, const char *message, bool newLine = true) const;

            static xSemaphoreHandle smSerialMutex; ///< Static handle to FreeRTOS mutex protecting changing settings of Serial and printing.
            static bool smIsSerialEnabled; ///< Static flag ensuring that <code>Serial.begin()</code> is called only once.

            // TODO before merge with main remove commented code/rollback atomic
            std::atomic<Level> mLogLevel; ///< Currently set logging level.
            // Level mLogLevel; ///< Currently set logging level.
            // xSemaphoreHandle mLogLevelMutex; ///< Handle to FreeRTOS mutex protecting <code>mLogLevel</code>.
        };
    }
}