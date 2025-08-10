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
        }

        void Logger::error(const char *name, const char *message) {
            writeLog(Level::ERROR, name, message);
        }
        void Logger::errorWithValue(const char *name, const char *message, const uint8_t value) {

        }
        void Logger::errorWithValue(const char *name, const char *message, const uint8_t *value) {

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
                info("Logger Class", message);
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
                    char errorMessage[38];
                    sprintf(errorMessage, "Incorrect log level: %i, log ignored.", level);
                    error("Logger Method", errorMessage);
                    return false;
            }
        }

        void Logger::writeLog(const Level level, const char *name, const char *message) {
            constexpr uint8_t logTypeLength = 10;

            // protections
            if (mLogLevel < level) return;
            char logType[logTypeLength];
            if (!logLevelToString(logType, level)) return;

            const uint16_t size = snprintf(
                nullptr,
                0,
                "%s [%s] %s",
                logType,
                name,
                message
            );
            char log[size + 1];
            sprintf(
                log,
                "%s [%s] %s",
                logType,
                name,
                message
            );
            Serial.println(log);
        }
    }
}