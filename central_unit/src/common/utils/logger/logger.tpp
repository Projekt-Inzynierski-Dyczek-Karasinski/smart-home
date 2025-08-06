#pragma once

namespace SmartHome::Utils {
    template<typename... Args>
    std::string Logger::formatMessage(const char *format, Args... args) {
        const int size = std::snprintf(nullptr, 0, format, args...);

        std::string message(size + 1, '\0');
        std::snprintf(message.data(), message.size(), format, args...); //TODO limit message size?
        message.resize(size);

        return message;
    }

    template<typename... Args>
    void Logger::logf(const LogLevels::Level level, const char *format, Args... args) {
        if (level > mLevel) return;
        Logger::writeLog(level, Logger::formatMessage(format, args...));
    }

    template<typename... Args>
    void Logger::criticalf(const char *format, Args... args) {
        Logger::logf(LogLevels::Level::CRITICAL, format, args...);
    }

    template<typename... Args>
    void Logger::errorf(const char *format, Args... args) {
        Logger::logf(LogLevels::Level::ERROR, format, args...);
    }

    template<typename... Args>
    void Logger::warningf(const char *format, Args... args) {
        Logger::logf(LogLevels::Level::WARNING, format, args...);
    }

    template<typename... Args>
    void Logger::infof(const char *format, Args... args) {
        Logger::logf(LogLevels::Level::INFO, format, args...);
    }

    template<typename... Args>
    void Logger::debugf(const char *format, Args... args) {
        Logger::logf(LogLevels::Level::DEBUG, format, args...);
    }
}
