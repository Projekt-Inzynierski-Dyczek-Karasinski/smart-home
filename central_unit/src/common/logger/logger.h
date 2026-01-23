#pragma once

#include <mutex>
#include <string>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>


namespace SmartHome::Utils {
    /**
     * @brief Log levels definition and utilities.
     */
    struct LogLevels {
        /**
         * @brief Enumeration of available log levels.
         */
        enum class Level :uint8_t {
            NONE = 0,
            CRITICAL = 1,
            ERROR = 2,
            WARNING = 3,
            INFO = 4,
            DEBUG = 5
        };

        static constexpr auto minLevel = Level::NONE;

        static constexpr auto maxLevel = Level::DEBUG;

        static constexpr auto defaultLevel = Level::ERROR;

        /**
         * @brief Convert log level to string representation.
         *
         * @param level log level to convert.
         * @return String view of the level name.
         */
        static constexpr std::string_view toString(const Level level) {
            switch (level) {
                case Level::NONE: return "INVALID";
                case Level::CRITICAL: return "CRITICAL";
                case Level::ERROR: return "ERROR";
                case Level::WARNING: return "WARNING";
                case Level::INFO: return "INFO";
                case Level::DEBUG: return "DEBUG";
                default: return "UNDEFINED";
            }
        }

        /**
         * @brief Convert integer to log level with bounds checking.
         *
         * @param level Integer value to convert.
         * @return Corresponding Level enum value, clamped to valid range.
         */
        static Level toLevel(int level) {
            const auto minLevelValue = static_cast<uint8_t>(level);

            if (level >= minLevelValue && level <= static_cast<int>(maxLevel)) {
                return static_cast<Level>(level);
            }
            if (level < minLevelValue) {
                return minLevel;
            }
            return maxLevel;
        }
    };

    /**
     * @brief Synchronous logger for SmartHome system.
     *
     * @details Logger provides logging to console and/or file with multiple verbosity levels.
     *          Supports printf-style formating through template methods.
     *
     * @warning Not thread-safe, use AsyncLogger for thread-safe logging.
     */
    class Logger {
    public:
        /**
         * @brief Logger configuration struct.
         */
        struct Config {
            LogLevels::Level logLevel = LogLevels::defaultLevel; ///< Verbosity level
            bool enableConsoleLogOutput = true; ///< Enable/disable console output

            struct LogFile {
                bool enabled = true; ///< Enable/disable file logging
                bool createNew = true; ///< Clear existing / creates new file writes log file header
                bool archiveOld = true; ///< Archives existing file to .old //TODO Add more advanced archiving options
                std::string path = "/var/log/smarthome/smarthome.log"; ///< Log file path
            } logFile;
        };

        /**
         * @brief Construct logger with default setting.
         *
         * @note Console logging enabled and file logging disabled by default.
         */
        Logger();

        /**
         * @brief Virtual destructor ensures proper cleanup.
         */
        virtual ~Logger();

        /**
         * @brief Apply config to logger.
         *
         * @details Sets all options present in Logger::Config.
         *
         * @param config Configuration struct to apply.
         */
        void applyConfig(const Config &config);

        /**
         * @brief Get current logger configuration.
         *
         * @return Struct with current configuration values.
         */
        Config getConfig() const;

        /**
         * @brief Enable file logging with specified parameters.
         *
         * @details Handles creating and archiving log files according to set options.
         *
         * @param filePath Path to log file.
         * @param createNewFile Optional: override config for creating new log file.
         * @param archiveOldFile Optional: override config for archiving.
         *
         * @note Logs warning and returns if mEnableFileLogging flag is set to false.
         */
        void enableFileLogging(std::string_view filePath,
                               std::optional<bool> createNewFile = std::nullopt,
                               std::optional<bool> archiveOldFile = std::nullopt);

        /**
         *  @brief Enable file logging using stored configuration.
         *
         *  @details Convenience method meant to be used after applying config with applyConfig() method.
         */
        void enableFileLoggingIfConfigured();

        /**
         * @brief Disables file logging and closes file stream if open.
         */
        void disableFileLogging();

        /**
         * @brief Enables console output.
         */
        void enableConsoleLogging();

        /**
         * @brief Disables console output.
         */
        void disableConsoleLogging();

        /**
         * @brief Archive current log file. //TODO add more advanced archiving
         */
        void archiveOldLogFile();

        /**
         * @brief Creates new log file with header.
         *
         * @details Creates parent directory if needed.
         *          Clears existing file or creates new one, writes timestamp header.
         */
        void setupNewLogFile();

        /**
         * @brief Log message with specified level.
         *
         * @param level Verbosity level of the message.
         * @param message Message to log.
         *
         * @note Virtual to allow AsyncLogger override.
         *       Message is ignored if level > current log level.
         */
        virtual void log(LogLevels::Level level, const std::string &message);

        /**
         * @brief Log formatted message with specified level.
         *
         * @tparam Args Variadic template arguments for formatting.
         * @param level Severity level of the message.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         */
        template<typename... Args>
        void logf(LogLevels::Level level, const char *format, Args... args);

        /**
         * @brief Log critical error message.
         *
         * @param message Message to log at CRITICAL level.
         */
        void critical(const std::string &message);

        /**
         * @brief Log formatted critical error message.
         *
         * @tparam Args Variadic template arguments for formatting.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         */
        template<typename... Args>
        void criticalf(const char *format, Args... args);

        /**
         * @brief Log error message.
         *
         * @param message Message to log at ERROR level.
         */
        void error(const std::string &message);

        /**
         * @brief Log formatted error message.
         *
         * @tparam Args Variadic template arguments for formatting.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         */
        template<typename... Args>
        void errorf(const char *format, Args... args);

        /**
         * @brief Log warning message.
         *
         * @param message Message to log at WARNING level.
         */
        void warning(const std::string &message);

        /**
         * @brief Log formatted warning message.
         *
         * @tparam Args Variadic template arguments for formatting.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         */
        template<typename... Args>
        void warningf(const char *format, Args... args);

        /**
         * @brief Log informational message.
         *
         * @param message Message to log at INFO level.
         */
        void info(const std::string &message);

        /**
         * @brief Log formatted informational message.
         *
         * @tparam Args Variadic template arguments for formatting.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         */
        template<typename... Args>
        void infof(const char *format, Args... args);

        /**
         * @brief Log debug message.
         *
         * @param message Message to log at DEBUG level.
         */
        void debug(const std::string &message);

        /**
         * @brief Log formatted debug message.
         *
         * @tparam Args Variadic template arguments for formatting.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         */
        template<typename... Args>
        void debugf(const char *format, Args... args);

        /**
         * @brief Check if file logging is currently active.
         *
         * @return True if file stream is open and logging, false otherwise.
         */
        bool isFileLoggingEnabled() const;
        /**
         * @brief Check if console logging is enabled.
         *
         * @return True if console output is enabled, false otherwise.
         */
        bool isConsoleLoggingEnabled() const;

        /**
         * @brief Check if file logging is set to enable.
         *
         * @return True if file logging should be enabled (config flag).
         */
        bool getEnableFileLogging() const;

        /**
         * @brief Set log level.
         *
         * @param level New level for logging.
         *
         * @note Messages with level > this value will be ignored.
         */
        void setLevel(LogLevels::Level level);

        /**
         * @brief Get current log level.
         *
         * @return Current log level.
         */
        LogLevels::Level getLevel() const;

        /**
         * @brief Get current log file path.
         *
         * @return Reference to log file path string.
         */
        const std::string &getFilePath();

    protected:
        /**
         * @brief Write log message to configured outputs.
         *
         * @details Formats message and writes to console and/or file based on configuration.
         *          Console uses cout for INFO/DEBUG/WARNING, cerr for ERROR/CRITICAL.
         *
         * @param level Message verbosity level.
         * @param message Message to write.
         *
         */
        void writeLog(LogLevels::Level level, const std::string &message);

        /**
         * @brief Get current timestamp string.
         *
         * @return Formatted timestamp string "YYYY-MM-DD HH:MM:SS".
         *
         */
        static std::string getTimeStamp();

        /**
         * @brief Prepare log message with level prefix.
         *
         * @details Formats message as "[LEVEL] message\n".
         *
         * @param level Message verbosity level.
         * @param message Raw message text.
         *
         * @return String containing formated message.
         */
        static std::string prepareLogMessage(LogLevels::Level level, std::string_view message);

        /**
         * @brief Format message using printf-style formatting.
         *
         * @tparam Args Variadic template arguments.
         * @param format Printf-style format string.
         * @param args Arguments for format string.
         *
         * @return Formatted string.
         */
        template<typename... Args>
        static std::string formatMessage(const char *format, Args... args);

        /// Logger level, enables logging messages of set level and levels bellow it.
        LogLevels::Level mLevel = LogLevels::defaultLevel;

        // Flags
        bool mEnableFileLogging = false; ///< Config flag used in enableFileLoggingIfConfigured
        bool mIsFileLoggingEnabled = false; ///< File logging is active and events will be writen to log file
        bool mEnableConsoleLogOutput = true; ///< Flag for suppressing console output
        bool mCreateNewLogFile = true; ///< Create new file overwriting old if true, otherwise append to existing file
        bool mArchiveOldLogFile = true; ///< Copy log file if it already exists to [log_file_path].old

        // Log file
        std::string mFilePath; ///< Path to log file
        std::ofstream mFileStream; ///< Output  file stream for logging
    };
}

#include "logger.tpp"
