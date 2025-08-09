#include "logger.h"

#include <Arduino.h>

namespace Utils {
    namespace Logging {
        Logger::Logger(const Level level) : mLogLevel(level) {
            if (mLogLevel == Level::NONE) return;
            Serial.begin(9600);
        }

        void Logger::writeLog(const Level level, const char *name, const char *functionName, const char *message) {
            if (mLogLevel < level) return;
            char logType[8];
            switch (level) {
                case Level::ERROR: strcpy(logType, "ERROR"); break;
                case Level::WARNING: strcpy(logType, "WARNING"); break;
                case Level::INFO: strcpy(logType, "INFO"); break;
                case Level::DEBUG: strcpy(logType, "DEBUG"); break;
                default: ;
            }

            char logMessage[200];
            sprintf(
                logMessage,
                "%s %s In %s -> %s",
                logType,
                name,
                functionName,
                message
            );
            Serial.println(logMessage);
        }

    }
}