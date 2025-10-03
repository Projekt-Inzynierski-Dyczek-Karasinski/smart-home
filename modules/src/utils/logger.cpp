#include "logger.h"

#include <Arduino.h>
#include <nlohmann/json.hpp>

#include "../config/logger_config.h"
#include "utils/uint8_array_handlers.h"
#include "universal_module_system/data_manager.h"

namespace uah = Utils::ArrayHandlers;
namespace ums = UniversalModuleSystem;

namespace Utils {
    namespace Logging {
        xSemaphoreHandle Logger::smSerialMutex = xSemaphoreCreateMutex();
        bool Logger::smIsSerialEnabled = false;

        Logger::Logger(Level level) {
            // TODO before merge with main remove commented code/rollback atomic
            // mLogLevelMutex = xSemaphoreCreateMutex();
            // xSemaphoreTake(mLogLevelMutex, portMAX_DELAY);
            // mLogLevel = level;
            // xSemaphoreGive(mLogLevelMutex);
            mLogLevel.store(level);

            beginSerial();

            #ifndef DEBUG_MODE
                ums::DataManager& dataManager = ums::DataManager::getInstance();
                // TODO change path
                if (dataManager.isFileExists("/main")) {
                    nl::json loggerData = dataManager.load("/main");
                    if (loggerData["disableLogs"] == true) {
                        mLogLevel.store(Level::NONE);
                    }
                } else {
                    nl::json loggerData;
                    loggerData["disableLogs"] = true;
                    dataManager.save("/main", loggerData);
                }
            #endif

            if (getLogLevel() == Level::DEBUG) {
                warning("Logger Level DEBUG", "Logger is set with Level::DEBUG.\n If this is main Logger instance (that is used for all components),\n Logger will print a huge amount of messages,\n consider making new instance to debug only part of program,\n otherwise that may cause unintended behaviour (including panic core).\n");
            }
        }

        // TODO before merge with main remove commented code/rollback atomic
        // Logger::~Logger() {
        //     vSemaphoreDelete(mLogLevelMutex);
        // }

        Level Logger::getLogLevel() const {
            // TODO before merge with main remove commented code/rollback atomic
            // xSemaphoreTake(mLogLevelMutex, portMAX_DELAY);
            // const Level level = mLogLevel;
            // xSemaphoreGive(mLogLevelMutex);
            // return level;
            return mLogLevel.load();
        }

        void Logger::setLogLevel(const Level newLevel) {
            xSemaphoreTake(smSerialMutex, portMAX_DELAY);
            char logLevel[m_LOG_TYPE_LENGTH];
            if (logLevelToString(logLevel, newLevel)) {
                constexpr char message[] = "[INFO] [Logger] Changing log level to: ";
                const size_t messageSize = snprintf(nullptr, 0, "%s, %s", message, logLevel);
                char messageBuffer[messageSize];
                sprintf(messageBuffer, " Changing log level to: %s", logLevel);
                Serial.println(messageBuffer);
            }
            mLogLevel.store(newLevel);
            xSemaphoreGive(smSerialMutex);
        }

        bool Logger::getIsSerialEnabled() {
            xSemaphoreTake(smSerialMutex, portMAX_DELAY);
            const bool isSerialEnabled = smIsSerialEnabled;
            xSemaphoreGive(smSerialMutex);
            return isSerialEnabled;
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
            xSemaphoreTake(smSerialMutex, portMAX_DELAY);
            if (!smIsSerialEnabled) {
                Serial.begin(TERMINAL_BAUD_RATE);
                Serial.println();
                smIsSerialEnabled = true;
                xSemaphoreGive(smSerialMutex);

                char message[35];
                sprintf(message, "Serial began with baudrate %i.", TERMINAL_BAUD_RATE);
                info("Logger Class", message);
            } else {
                xSemaphoreGive(smSerialMutex);
            }
        }

        bool Logger::logLevelToString(char *buffer, const Level level) {
            switch (level) {
                case Level::ERROR: strcpy(buffer, "<ERROR>"); return true;
                case Level::WARNING: strcpy(buffer, "<WARNING>"); return true;
                case Level::INFO: strcpy(buffer, "[INFO]"); return true;
                case Level::DEBUG: strcpy(buffer, "[DEBUG]"); return true;
                case Level::NONE: strcpy(buffer, "NONE"); return true;
                default:
                    char errorMessage[38];
                    sprintf(errorMessage, "Incorrect log level: %i, log ignored.", level);
                    error("Logger Method", errorMessage);
                    return false;
            }
        }

        void Logger::log(const Level level, const char *name, const char *message) {
            // protections
            if (getLogLevel() < level) return;
            char logType[m_LOG_TYPE_LENGTH];
            if (!logLevelToString(logType, level)) return;

            // write log
            xSemaphoreTake(smSerialMutex, portMAX_DELAY);
            writeLog(logType, name, message);
            xSemaphoreGive(smSerialMutex);
        }
        void Logger::log(const Level level, const char *name, const char *message, const int value) {
            // protections
            if (getLogLevel() < level) return;
            char logType[m_LOG_TYPE_LENGTH];
            if (!logLevelToString(logType, level)) return;

            // write log
            xSemaphoreTake(smSerialMutex, portMAX_DELAY);
            writeLog(logType, name, message, false);
            Serial.println((int)value);
            xSemaphoreGive(smSerialMutex);
        }
        void Logger::log(const Level level, const char *name, const char *message, const uint8_t *values, const uint8_t len, const bool isAscii) {
            // protections
            if (getLogLevel() < level) return;
            char logType[m_LOG_TYPE_LENGTH];
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
                xSemaphoreTake(smSerialMutex, portMAX_DELAY);
                writeLog(logType, name, message, false);
                Serial.println(valueBuffer);
            } else {
                // special log for rare cases
                xSemaphoreTake(smSerialMutex, portMAX_DELAY);
                writeLog(logType, name, message, false);
                char currentPrint[5];
                for (uint8_t i = 0; i < len; i++) {
                    sprintf(currentPrint, "%i ", (int)values[i]);
                    Serial.print(currentPrint);
                }
                Serial.println();
            }
            xSemaphoreGive(smSerialMutex);
        }

        void Logger::writeLog(const char *logType, const char *name, const char *message, const bool newLine) const {
            const uint16_t size = snprintf(nullptr, 0, "%s [%s] %s", logType, name, message) + 1;
            char log[size];
            sprintf(log, "%s [%s] %s", logType, name, message);

            if (newLine) {
                Serial.println(log);
            } else {
                Serial.print(log);
            }
        }
    }
}