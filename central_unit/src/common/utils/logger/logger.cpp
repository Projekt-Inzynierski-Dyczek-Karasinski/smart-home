#include "logger.h"

#include <chrono>
#include <filesystem>
#include <iomanip>

namespace sf = std::filesystem;

namespace SmartHome::Utils {
    using LL = LogLevels;
    //TODO add advanced logging options (log file rotation, compression, etc)

    Logger::Logger() {
        try {
            mBuffer.reserve(ms_BUFFER_RESERVED_CHARS);
        } catch (std::exception &e) {
            std::cerr << "[LOGGER_ERROR] " << e.what() << std::flush;
        }
    }

    Logger::~Logger() {
        if (mIsFileLoggingEnabled && mFileStream.is_open()) {
            mFileStream.close();
        }
    }

    void Logger::applyConfig(const Config &config) {
        mLevel = config.logLevel;
        if (!config.enableConsoleLogOutput) disableConsoleLogging();
        mEnableFileLogging = config.logFile.enabled;
        mCreateNewLogFile = config.logFile.createNew;
        mArchiveOldLogFile = config.logFile.archiveOld;
        mFilePath = config.logFile.path;
    }

    Logger::Config Logger::getConfig() const {
        Config config;
        config.logLevel = mLevel;
        config.enableConsoleLogOutput = mEnableConsoleLogOutput;
        config.logFile.enabled = mEnableFileLogging;
        config.logFile.createNew = mCreateNewLogFile;
        config.logFile.archiveOld = mArchiveOldLogFile;
        config.logFile.path = mFilePath;
        return config;
    }

    void Logger::enableFileLogging(const std::string_view filePath,
                                   const std::optional<bool> createNewFile,
                                   const std::optional<bool> archiveOldFile) {
        if (!mEnableFileLogging) {
            warning("[LOGGER] File logging disabled");
            return;
        }
        if (mFileStream.is_open()) {
            mFileStream.close();
        }

        mFilePath = filePath;
        if (archiveOldFile.value_or(mArchiveOldLogFile)) archiveOldLogFile();

        if (createNewFile.value_or(mCreateNewLogFile)) setupNewLogFile();
        else {
            mFileStream.open(mFilePath, std::ios::app);
            info("[LOGGER] New stream, logging continued");
        }

        mIsFileLoggingEnabled = true;
    }

    void Logger::enableFileLoggingIfConfigured() {
        if (mEnableFileLogging) {
            enableFileLogging(mFilePath, mCreateNewLogFile, mArchiveOldLogFile);
        }
    }

    void Logger::disableFileLogging() {
        mIsFileLoggingEnabled = false;
        if (mFileStream.is_open()) mFileStream.close();
    }

    void Logger::enableConsoleLogging() {
        mEnableConsoleLogOutput = true;
    }

    void Logger::disableConsoleLogging() {
        mEnableConsoleLogOutput = false;
    }

    void Logger::archiveOldLogFile() {
        std::error_code ec;
        std::filesystem::copy_file(mFilePath, mFilePath + ".old", sf::copy_options::overwrite_existing, ec);
        if (ec) {
            switch (ec.value()) {
                case static_cast<int>(std::errc::no_such_file_or_directory):
                    error("[LOGGER] Failed to archive old log file: no such file found");
                    break;
                case static_cast<int>(std::errc::permission_denied):
                    error("[Logger] Failed to archive old log file: permission denied]");
                    break;
                default:
                    errorf("[Logger] Failed to archive old log file: unexpected error (%s)", ec.message().c_str());
            }
        }
    }

    void Logger::setupNewLogFile() {
        auto parentPath = std::filesystem::path(mFilePath).parent_path();
        if (std::filesystem::exists(parentPath)) {
            // Clear existing file or create new file
            std::ofstream(mFilePath, std::ios::trunc);
        } else {
            std::filesystem::create_directory(parentPath);
        }
        // Open empty file in append mode
        mFileStream.open(mFilePath, std::ios::app);
        // Add header
        mBuffer = "Log started at " + getTimeStamp() + "\n\n";
        mFileStream << mBuffer << std::flush;
        info("[Logger] New log file created");
    }


    void Logger::log(const LogLevels::Level level, const std::string &message) {
        if (level > mLevel) return;
        writeLog(level, message);
    }

    void Logger::critical(const std::string &message) {
        log(LL::Level::CRITICAL, message);
    }

    void Logger::error(const std::string &message) {
        log(LL::Level::ERROR, message);
    }

    void Logger::warning(const std::string &message) {
        log(LL::Level::WARNING, message);
    }

    void Logger::info(const std::string &message) {
        log(LL::Level::INFO, message);
    }

    void Logger::debug(const std::string &message) {
        log(LL::Level::DEBUG, message);
    }

    bool Logger::isFileLoggingEnabled() const {
        return mIsFileLoggingEnabled;
    }

    bool Logger::isConsoleLoggingEnabled() const {
        return mEnableConsoleLogOutput;
    }

    bool Logger::getEnableFileLogging() const {
        return mEnableFileLogging;
    }

    void Logger::setLevel(const LogLevels::Level level) {
        mLevel = level;
    }

    LogLevels::Level Logger::getLevel() const {
        return mLevel;
    }


    const std::string &Logger::getFilePath() {
        return mFilePath;
    }

    void Logger::writeLog(const LogLevels::Level level,
                          const std::string &message) {
        prepareLogMessage(level, message);


        if (mEnableConsoleLogOutput) {
            if (level == LL::Level::CRITICAL || level == LL::Level::ERROR) {
                std::cerr << mBuffer << std::flush;
            } else if (level == LogLevels::Level::WARNING || level == LL::Level::INFO || level == LL::Level::DEBUG) {
                std::cout << mBuffer << std::flush;
            }
        }

        if (mIsFileLoggingEnabled && mFileStream.is_open()) {
            mBuffer = getTimeStamp() + ' ' + mBuffer;
            mFileStream << mBuffer << std::flush;
        }
    }

    inline std::string Logger::getTimeStamp() {
        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuffer;
        std::stringstream ss;
        ss << std::put_time(localtime_r(&nowTime, &tmBuffer), "%Y-%m-%d %H:%M:%S"); //Linux only
        return ss.str();
    }

    void Logger::prepareLogMessage(const LL::Level level, const std::string_view message) {
        mBuffer.clear();
        mBuffer += '[' + std::string(LL::toString(level)) + "] " + std::string(message) + '\n';
    }
}
