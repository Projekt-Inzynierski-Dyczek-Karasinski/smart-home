#include "async_logger.h"

namespace SmartHome::Utils {
    using LL = LogLevels;

    AsyncLogger::AsyncLogger(const std::shared_ptr<Logger> &logger, ba::io_context &ioContext) : mIoStrand(ioContext) {
        auto config = logger->getConfig();
        config.logFile.createNew =false;
        config.logFile.archiveOld=false;
        applyConfig(config);
        enableFileLoggingIfConfigured();

    }

    AsyncLogger::AsyncLogger(ba::io_context &ioContext): mIoStrand(ioContext) {
    }

    AsyncLogger::~AsyncLogger() {
        if (mIsFileLoggingEnabled && mFileStream.is_open()) {
            mFileStream.close();
        }
    }

    void AsyncLogger::log(const LogLevels::Level level, const std::string &message) {
        if (level > mLevel) return;
        ba::post(mIoStrand, [this, level, message] {
            writeLog(level, message);
        });
    }
}
