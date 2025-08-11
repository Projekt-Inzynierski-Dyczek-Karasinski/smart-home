#include "logger.h"

#include <Arduino.h>

#include "smart_home_config.h"
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

        Level Logger::getLogLevel() const {
            return mLogLevel;
        }

        void Logger::error(const char *name, const char *message) {
            log(Level::ERROR, name, message);
        }
        void Logger::errorv(const char *name, const char *message, const int value) {
            log(Level::ERROR, name, message, value);
        }
        void Logger::errora(const char *name, const char *message, const uint8_t *values, const uint8_t len, const bool isAscii) {
            log(Level::ERROR, name, message, values, len, isAscii);
        }

        void Logger::warning(const char *name, const char *message) {
            log(Level::WARNING, name, message);
        }
        void Logger::warningv(const char *name, const char *message, const int value) {
            log(Level::WARNING, name, message, value);
        }
        void Logger::warninga(const char *name, const char *message, const uint8_t *values, const uint8_t len, const bool isAscii) {
            log(Level::WARNING, name, message, values, len, isAscii);
        }

        void Logger::info(const char *name, const char *message) {
            log(Level::INFO, name, message);
        }
        void Logger::infov(const char *name, const char *message, const int value) {
            log(Level::INFO, name, message, value);
        }
        void Logger::infoa(const char *name, const char *message, const uint8_t *values, const uint8_t len, const bool isAscii) {
            log(Level::INFO, name, message, values, len, isAscii);
        }

        void Logger::debug(const char *name, const char *message) {
            log(Level::DEBUG, name, message);
        }
        void Logger::debugv(const char *name, const char *message, const int value) {
            log(Level::DEBUG, name, message, value);
        }
        void Logger::debuga(const char *name, const char *message, const uint8_t *values, const uint8_t len, const bool isAscii) {
            log(Level::DEBUG, name, message, values, len, isAscii);
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

        void Logger::log(const Level level, const char *name, const char *message) {
            constexpr uint8_t logTypeLength = 10;
            // protections
            if (mLogLevel < level) return;
            char logType[logTypeLength];
            if (!logLevelToString(logType, level)) return;

            // write log
            writeLog(logType, name, message);
        }
        void Logger::log(const Level level, const char *name, const char *message, const int value) {
            constexpr uint8_t logTypeLength = 10;
            // protections
            if (mLogLevel < level) return;
            char logType[logTypeLength];
            if (!logLevelToString(logType, level)) return;

            // write log
            writeLog(logType, name, message, false);
            Serial.println((int)value);
        }
        void Logger::log(const Level level, const char *name, const char *message, const uint8_t *values, const uint8_t len, const bool isAscii) {
            constexpr uint8_t logTypeLength = 10;
            // protections
            if (mLogLevel < level) return;
            char logType[logTypeLength];
            if (!logLevelToString(logType, level)) return;

            if (isAscii) {
                // calc len of data in values array and convert uint8_t array into char array
                const uint8_t realLen = uah::calcLenOfDataInArray(values, len) + 1;
                char valueBuffer[realLen];
                for (uint8_t i = 0; i < realLen; i++) {
                    valueBuffer[i] = (char)values[i];
                }
                valueBuffer[realLen - 1] = '\0';

                // write log
                writeLog(logType, name, message, false);
                Serial.println(valueBuffer);
            } else {
                // special log for rare cases
                writeLog(logType, name, message, false);
                char currentPrint[5];
                for (uint8_t i = 0; i < len; i++) {
                    sprintf(currentPrint, "%i ", (int)values[i]);
                    Serial.print(currentPrint);
                }
                Serial.println();
            }
        }

        void Logger::writeLog(const char *logType, const char *name, const char *message, const bool newLine) const {
            const uint16_t size = snprintf(
                nullptr,
                0,
                "%s [%s] %s",
                logType,
                name,
                message
            ) + 1;
            char log[size];
            sprintf(
                log,
                "%s [%s] %s",
                logType,
                name,
                message
            );

            if (newLine) {
                Serial.println(log);
            } else {
                Serial.print(log);
            }
        }
    }
}