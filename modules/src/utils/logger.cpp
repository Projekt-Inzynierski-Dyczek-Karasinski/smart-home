#include "logger.h"

#include <Arduino.h>

#include "utils/uint8_array_handlers.h"

namespace uah = Utils::ArrayHandlers;

namespace Utils {
    namespace Logging {
        xSemaphoreHandle Logger::smSerialBeginMutex = xSemaphoreCreateMutex();
        bool Logger::smIsSerialBegin = false;

        Logger::Logger(const Level level) : mLogLevel(level) {
            if (mLogLevel == Level::NONE) return;
            beginSerial();
            info("Logger", "Logger initialized.");
        }

        void Logger::error(const char *name, const char *message) {
            writeLog(Level::ERROR, name, message);
        }
        void Logger::warning(const char *name, const char *message) {
            writeLog(Level::WARNING, name, message);
        }
        void Logger::info(const char *name, const char *message) {
            writeLog(Level::INFO, name, message);
        }
        void Logger::debug(const char *name, const char *message) {
            writeLog(Level::DEBUG, name, message);
        }

        void Logger::beginSerial()  {
            xSemaphoreTake(smSerialBeginMutex, portMAX_DELAY);
            if (!smIsSerialBegin) {
                Serial.begin(TERMINAL_BAUD_RATE);
                Serial.println();
                smIsSerialBegin = true;

                char message[35];
                sprintf(message, "Serial began with baudrate %i.", TERMINAL_BAUD_RATE);
                info("Logger", message);
            }
            xSemaphoreGive(smSerialBeginMutex);
        }

        bool Logger::logLevelToString(char *buffer, const Level level) {
            switch (level) {
                case Level::ERROR: strcpy(buffer, "[ERROR]"); return true;
                case Level::WARNING: strcpy(buffer, "[WARNING]"); return true;
                case Level::INFO: strcpy(buffer, "[INFO]"); return true;
                case Level::DEBUG: strcpy(buffer, "[DEBUG]"); return true;
                default:
                    error("LOGGER", "Bad level, log ignored.");
                    return false;
            }
        }

        void Logger::writeLog(const Level level, const char *name, const char *message) {
            constexpr uint8_t logInfoLength = 32;
            constexpr uint8_t logTypeLength = 10;
            constexpr uint8_t logInfoConstCharsLength = 4;
            constexpr uint8_t maxLengthOfName = logInfoLength - logTypeLength - logInfoConstCharsLength;

            // protections
            if (mLogLevel < level) return;
            if (uah::calcLenOfDataInArray(name, maxLengthOfName + 1) > maxLengthOfName) {
                error("LOGGER", "Name of log too long, log ignored.");
                return;
            }
            char logType[logTypeLength];
            if (!logLevelToString(logType, level)) return;

            // prepare first half of log
            char logInfo[logInfoLength];
            sprintf(
                logInfo,
                "%s [%s] ",
                logType,
                name
            );

            // print log, separate prints for saving memory
            Serial.print(logInfo);
            Serial.println(message);
        }
    }
}