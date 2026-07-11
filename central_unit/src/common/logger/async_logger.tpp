#pragma once

namespace SmartHome::Utils {
    template<typename... Args>
    void AsyncLogger::logf(const LogLevels::Level level, const char *format, Args... args) {
        if (level > mLevel) return;

        std::string message = AsyncLogger::formatMessage(format, args...);

        ba::post(mIoStrand, [this, level, message] {
            writeLog(level, message);
        });
    }

    template<typename... Args>
    void AsyncLogger::criticalf(const char *format, Args... args) {
        AsyncLogger::logf(LogLevels::Level::Critical, format, args...);
    }

    template<typename... Args>
    void AsyncLogger::errorf(const char *format, Args... args) {
        AsyncLogger::logf(LogLevels::Level::Error, format, args...);
    }

    template<typename... Args>
    void AsyncLogger::warningf(const char *format, Args... args) {
        AsyncLogger::logf(LogLevels::Level::Warning, format, args...);
    }

    template<typename... Args>
    void AsyncLogger::infof(const char *format, Args... args) {
        AsyncLogger::logf(LogLevels::Level::Info, format, args...);
    }

    template<typename... Args>
    void AsyncLogger::debugf(const char *format, Args... args) {
        AsyncLogger::logf(LogLevels::Level::Debug, format, args...);
    }
}
