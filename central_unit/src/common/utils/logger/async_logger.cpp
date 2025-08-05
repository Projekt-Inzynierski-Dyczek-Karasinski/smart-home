#include "async_logger.h"

namespace SmartHome::Utils {
    using LL = LogLevels;

    AsyncLogger::AsyncLogger(const std::shared_ptr<Logger> &logger, ba::io_context &ioContext) : mIoStrand(ioContext) {
        try {
            mBuffer.reserve(ms_BUFFER_RESERVED_CHARS);
        } catch (std::exception &e) {
            std::cerr << "[LOGGER_ERROR] " << e.what() << std::endl;
        }

        auto config = logger->getConfig();
        config.logFile.createNew =false;
        config.logFile.archiveOld=false;
        applyConfig(config);
        enableFileLoggingIfConfigured();

    }

    AsyncLogger::AsyncLogger(ba::io_context &ioContext): mIoStrand(ioContext) {
        try {
            mBuffer.reserve(ms_BUFFER_RESERVED_CHARS);
        } catch (std::exception &e) {
            std::cerr << "[LOGGER_ERROR] " << e.what() << std::endl;
        }
    }

    AsyncLogger::~AsyncLogger() {
        if (mIsFileLoggingEnabled && mFileStream.is_open()) {
            mFileStream.close();
        }
    }

    void AsyncLogger::log(const LogLevels::Level level, const std::string &message) {
        if (level > mLevel) return;
        ba::post(mIoStrand, [this, level, message]() {
            writeLog(level, message);
        });
    }
}
