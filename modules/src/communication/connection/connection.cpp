#include "connection.h"

#include "communication/communication.h"
#include "universal_module_system/power_manager/power_manager.h"
#include "universal_module_system/transducers/sensors/sensors_manager.h"

namespace Comms {
    // ============================ Public ============================
    Connection &Connection::getInstance(
        Communication *communication,
#ifdef CENTRAL_UNIT
        const std::shared_ptr<CentralUnitAddressing> &addressing,
#else
        const std::shared_ptr<ModuleAddressing> &addressing,
#endif
        const std::shared_ptr<HC12> &rfModule,
        const std::shared_ptr<ul::Logger> &logger
    ) {
        static Connection instance(communication, addressing, rfModule, logger);
        return instance;
    }

    // TODO !pr remove
    void Connection::messageDecider(const uint8_t receivedMessage[MESSAGE_SIZE]) {
        uint8_t sendBuffer[MESSAGE_SIZE];

        using CM = ConnectionMessages;
        using CME = ConnectionMessages::MessagesEnum;
        // TODO !mm remove test messages
        // TODO consider ignoring 2 first letters in if statements (these letters are already checked)
        switch (
            const auto it = CM::messagesMap.find((char *) receivedMessage);
            it != CM::messagesMap.end() ? it->second : CME::UNKNOWN
        ) {
            case CME::END:
                endConnection();
                break;

            case CME::AFFIRM:
                uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_END, MESSAGE_SIZE);
                mpCommunication->sendMessage(sendBuffer);
                endConnection();
                break;

            case CME::REPEAT_MESSAGE:
                repeatLastTransmittedMessage();
                break;

            case CME::TEST_EXECUTE:
                uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_AFFIRM, MESSAGE_SIZE);
                mpCommunication->sendMessage(sendBuffer);
                break;

            case CME::TEST_GET: {
                auto &sensorManager = ums::Transducers::SensorsManager::getInstance(mpLogger);
                const String res = sensorManager.getAllSensorsReport();

                uah::prepareBuffer(sendBuffer, res.c_str(), MESSAGE_SIZE);

                // uah::prepareBuffer(sendBuffer,  CM::s_CONNECTION_RE_TEST_GET, MESSAGE_SIZE);
                mpCommunication->sendMessage(sendBuffer);
                break;
            }

            case CME::RE_TEST_GET:
                uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_END, MESSAGE_SIZE);
                mpCommunication->sendMessage(sendBuffer);
                endConnection();
                break;

            case CME::GO_TO_DEEP_SLEEP:
                xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
                mIsDeepSleep = true;
                xSemaphoreGive(mConnectionDataMutex);
            case CME::GO_TO_NORMAL_SLEEP: {
                std::optional<uint32_t> sleepTime;
                try {
                    sleepTime = std::stoi((char *) &receivedMessage[strlen(CM::s_CONNECTION_GO_TO_NORMAL_SLEEP)]);
                } catch (...) {
                    sleepTime.reset(); // make sure that sleepTime is null if error occurs
                    mpLogger->error("Connection Message Decider", "Received sleep time is not a number.");
                }

                if (sleepTime.has_value()) {
                    xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
                    mSleepTime = sleepTime.value();
                    xSemaphoreGive(mConnectionDataMutex);

                    uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_RE_GO_TO_SLEEP, MESSAGE_SIZE);
                    mpCommunication->sendMessage(sendBuffer);
                } else {
                    xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
                    mIsDeepSleep = false; // clear mIsDeepSleep from case CME::GO_TO_DEEP_SLEEP
                    xSemaphoreGive(mConnectionDataMutex);

                    uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_NEGATIVE, MESSAGE_SIZE);
                    mpCommunication->sendMessage(sendBuffer);
                }
                break;
            }

            case CME::RE_GO_TO_SLEEP:
                uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_END, MESSAGE_SIZE);
                mpCommunication->sendMessage(sendBuffer);
                endConnection();
                break;

            case CME::CHANGE_CHANNEL: {
                std::optional<uint8_t> newChannel;
                try {
                    newChannel = std::stoi((char *) &receivedMessage[strlen(CM::s_CONNECTION_CHANGE_CHANNEL)]);
                } catch (...) {
                    newChannel.reset(); // make sure that newChannel is null if error occurs
                    mpLogger->error("Connection Message Decider", "Received rf channel is not a number.");
                }

                if (newChannel.has_value()) {
                    xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
                    mTmpChannel = newChannel.value();
                    xSemaphoreGive(mConnectionDataMutex);

                    uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_AFFIRM, MESSAGE_SIZE);
                    mpCommunication->sendMessage(sendBuffer);
                    mpRfModule->firstChangeRFChannel(newChannel.value());
                } else {
                    uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_NEGATIVE, MESSAGE_SIZE);
                    mpCommunication->sendMessage(sendBuffer);
                }

                break;
            }

            default:
                mpLogger->warninga(
                    "Connection Message Decider",
                    "Received custom receivedMessage.\nIgnored receivedMessage: ",
                    receivedMessage,
                    MESSAGE_SIZE
                );
                break;
        }
    }

    // TODO !pr change to messageDecider
    void Connection::messageDecider2(const uint8_t receivedMessage[MESSAGE_SIZE]) {
        uint8_t sendBuffer[MESSAGE_SIZE] = {};
        using CT = API::commandTypes;
        using ET = API::errorTypes;
        std::optional<API::CommandHandler> receivedCommand;

        // decode command and handle decoding failure
        try {
            receivedCommand.emplace(receivedMessage);
        } catch (std::exception &e) {
            mpLogger->error("Connection messageDecider", "Failed to decode received command.");
            mpLogger->error("Connection messageDecider", e.what());
            receivedCommand.reset();
        }
        if (!receivedCommand.has_value()) {
            std::optional<API::CommandHandler> errorCommand;
            try {
                errorCommand.emplace(CT::RESPONSE);
                errorCommand->addParameter(API::APIParameter((uint8_t) ET::BAD_COMMAND, true));
            } catch (std::exception &e) {
                mpLogger->error("Connection messageDecider", "Failed to create error command.");
                mpLogger->error("Connection messageDecider", e.what());
            }

            if (errorCommand.has_value()) {
                errorCommand->generateMessage(sendBuffer);
                mpCommunication->sendMessage(sendBuffer);
            }
            return;
        }

        //TODO !pr remove
        using CM = ConnectionMessages;
        using CME = ConnectionMessages::MessagesEnum;

        std::optional<API::CommandHandler> sendCommand;
        std::optional<uint32_t> uid;
        bool isDeepSleep = false;

        switch (receivedCommand->getCommandType()) {
            // TODO !pr move RESPONSE to group below
            case CT::RESPONSE: {
                mpLogger->info("MessageDecider TMP", "response: ");
                uah::printArrayAsInt(receivedMessage, MESSAGE_SIZE);

                try {
                    const uint8_t batteryRead = receivedCommand->getParameterValue<uint8_t>(1);
                    mpLogger->infov("MessageDecider TEST", "Battery read %: ", batteryRead);
                } catch (...) {}

                try {
                    sendCommand.emplace(CT::END);
                } catch (std::exception &e) {
                    sendCommand.reset();
                    mpLogger->error("MessageDecider CT::RESPONSE", e.what());
                }
                break;
            }

            case CT::ACKNOWLEDGE:
            case CT::NEGATIVE:
            case CT::REPING: {
                size_t size = snprintf(nullptr, 0, "Got %s", receivedCommand->getCommandTypeString().c_str());
                char warningMessage[size];
                sprintf(warningMessage, "Got %s", receivedCommand->getCommandTypeString().c_str());
                mpLogger->warning("Connection messageDecide", warningMessage);

                if (mConnectionTimeoutTimer != nullptr) {
                    xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
                }
                break;
            }

            case CT::REPEAT:
                mpLogger->verbose("MessageDecider", "CT::REPEAT");
                repeatLastTransmittedMessage();
                break;

            case CT::END:
                mpLogger->verbose("MessageDecider", "CT::END");
                endConnection();
                break;

            case CT::PING: {
                mpLogger->verbose("MessageDecider", "CT::PING");

                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                } catch (std::exception &e) {
                    mpLogger->error("MessageDecider CT::PING", e.what());
                    responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                    break;
                }

                try {
                    sendCommand.emplace(CT::REPING);
                    sendCommand->addParameter(API::APIParameter(uid.value()));
                } catch (std::exception &e) {
                    receivedCommand.reset();
                    mpLogger->error("MessageDecider CT::REPING", e.what());
                }
                break;
            }

            case CT::DEEP_SLEEP:
                isDeepSleep = true;
            case CT::SLEEP: {
                mpLogger->verbose("MessageDecider", "CT::DEEP_SLEEP");

                std::optional<uint32_t> sleepTime;
                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                    sleepTime = receivedCommand->getParameterValue<uint32_t>(1);
                } catch (std::exception &e) {
                    mpLogger->error("MessageDecider CT::SLEEP", "Did not get uid and/or sleepTime");
                    responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                    break;
                }

                xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
                mSleepTime = sleepTime.value();
                mIsDeepSleep = isDeepSleep;
                xSemaphoreGive(mConnectionDataMutex);

                try {
                    sendCommand.emplace(CT::RESPONSE);
                    sendCommand->addParameter(API::APIParameter(uid.value()));
                    sendCommand->addParameter(API::APIParameter(sleepTime.value()));
                } catch (std::exception &e) {
                    sendCommand.reset();
                    mpLogger->error("MessageDecider CT::DEEP_SLEEP", e.what());
                }
                break;
            }

            case CT::GET: {
                mpLogger->verbose("MessageDecider", "CT::GET");

                // get always required arguments for CT::GET
                std::optional<uint8_t> getType;
                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                    getType = receivedCommand->getParameterValue<uint8_t>(1);
                } catch (std::exception &e) {
                    responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                    break;
                }

                using GT = API::getTypes;
                switch (getType.value()) {
                    case (uint8_t)GT::SENSOR_VALUE: {
                        mpLogger->verbose("MessageDecider TMP", "CT::SENSOR_VALUE");
                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    case (uint8_t)GT::CONFIG_VALUE: {
                        mpLogger->verbose("MessageDecider TMP", "CT::CONFIG_VALUE");

                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    case (uint8_t)GT::SENSOR_LIST: {
                        mpLogger->verbose("MessageDecider TMP", "CT::SENSOR_LIST");

                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    case (uint8_t)GT::LOGS: {
                        mpLogger->verbose("MessageDecider TMP", "CT::LOGS");
                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    // TESTME errors
                    case (uint8_t)GT::BATTERY_STATE: {
                        constexpr uint8_t BATTERY_SENSOR_ID = 1;
                        mpLogger->verbose("MessageDecider TMP", "CT::BATTERY_STATE");
                        auto & sensorManager = ums::Transducers::SensorsManager::getInstance(mpLogger);

                        try {
                            sendCommand.emplace(CT::RESPONSE);
                        } catch (std::exception &e) {
                            sendCommand.reset();
                            mpLogger->error("MessageDecider GT::BATTERY_STATE", e.what());
                            break;
                        }
                        
                        try {
                            sendCommand->addParameter(API::APIParameter(uid.value()));
                            sendCommand->addParameter(sensorManager.getSensorReading(BATTERY_SENSOR_ID));
                        } catch (std::exception &e) {
                            sendCommand.reset();
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }
                        break;
                    }

                    default:  {
                        mpLogger->errorv("MessageDecider CT::GET", "Got unknown get type: ", getType.value());
                        responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                        break;
                    }
                }
                break;
            }

            case CT::SET: {
                mpLogger->verbose("MessageDecider", "CT::SET");

                // TODO add handling for set
                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                } catch (std::exception &e) {
                    uid.reset();
                }
                responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                break;
            }

            case CT::NOTIFY: {
                mpLogger->verbose("MessageDecider", "CT::NOTIFY");
                try {
                    sendCommand.emplace(CT::ACKNOWLEDGE);
                } catch (std::exception &e) {
                    receivedCommand.reset();
                    mpLogger->error("MessageDecider CT::NOTIFY", e.what());
                }
                break;
            }

            // TODO !pr remove
            // case CT::GET: {
            //     mpLogger->verbose("TEST", "get");
            //     try {
            //         constexpr uint8_t TMP_READING = 55;
            //         uint32_t receivedUID = receivedCommand->getParameterValue<uint32_t>(0);
            //         mpLogger->verbosev("TEST", "receivedUID: ", receivedUID);
            //         uint8_t sensorID = receivedCommand->getParameterValue<uint8_t>(1);
            //         mpLogger->verbosev("TEST", "sensorID: ", sensorID);
            //
            //         // TODO add reading data from sensor
            //         mpLogger->warning("Connection messageDecider", "Fake reading");
            //
            //         API::CommandHandler responseCommand(CT::RESPONSE);
            //         API::APIParameter<uint32_t> uid(receivedUID);
            //         API::APIParameter<uint8_t> reading(TMP_READING);
            //         responseCommand.addParameter(uid);
            //         responseCommand.addParameter(reading);
            //         responseCommand.generateMessage(sendBuffer);
            //
            //         mpCommunication->sendMessage(sendBuffer);
            //         uah::prepareBuffer(sendBuffer, CM::s_CONNECTION_END, MESSAGE_SIZE);
            //         mpCommunication->sendMessage(sendBuffer);
            //         endConnection();
            //     } catch (std::exception &e) {
            //         // TODO add sending error command
            //         mpLogger->error("Connection messageDecider2", e.what());
            //         endConnection();
            //     }
            //     break;
            // }

            default:
                mpLogger->errorv(
                    "Connection messageDecider",
                    "Received unknown command type: ",
                    (int)receivedCommand->getCommandType()
                );
                responseWithError(sendCommand, ET::UNKNOWN_COMMAND, uid);
                break;
        }

        // TODO !pr remove
        // if (!responseCommand.has_value()) {
        //     mpLogger->error("Connection MessageDecider", "ET::INTERNAL_ERROR");
        //     try {
        //         responseCommand.emplace(CT::RESPONSE);
        //         responseCommand->addParameter(API::APIParameter((uint8_t)ET::INTERNAL_ERROR, true));
        //     } catch (std::exception &e) {
        //         receivedCommand.reset();
        //         mpLogger->error("MessageDecider ET::INTERNAL_ERROR", e.what());
        //     }
        // }

        // send response if successfully created command
        if (sendCommand.has_value()) {
            sendCommand->generateMessage(sendBuffer);
            mpCommunication->sendMessage(sendBuffer);

            // TODO !mm remove
            if (sendCommand->getCommandType() == CT::END)
                endConnection();
        }
    }

    void Connection::responseWithError(std::optional<API::CommandHandler> &responseCommand, API::errorTypes errorType, const std::optional<uint32_t> uid) const {
        using CT = API::commandTypes;
        if (!uid.has_value()) {
            try {
                responseCommand.emplace(CT::RESPONSE);
                responseCommand->addParameter(API::APIParameter((uint8_t) errorType, true));
            } catch (std::exception &e) {
                responseCommand.reset();
                mpLogger->error("Connection messageDecider", e.what());
            }
           return;
        }

        try {
            responseCommand.emplace(CT::RESPONSE);
            responseCommand->addParameter(API::APIParameter(uid.value()));
            responseCommand->addParameter(API::APIParameter((uint8_t) errorType, true));
        } catch (std::exception &e) {
            responseCommand.reset();
            mpLogger->error("Connection messageDecider", e.what());
        }
    }

    void Connection::receivingHandle(const uint8_t ip) {
        if (mpAddressing->getIsAddressingInProgress()) return;

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        createConnectionTimer();
        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        if (!mIsConnected) {
            auto &powerManager = ums::PowerManager::getInstance(mpLogger);
            powerManager.disableAutoSleep();

            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpAddressing->setProtocolIPAddress(ip); // do anything only for Central Unit
        }
        // if received any message while rf was changed, save rf channel
        if (mTmpChannel.has_value()) {
            mpAddressing->changeRfChannel(mTmpChannel.value());
            mTmpChannel.reset();
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::sendingHandle(const uint8_t message[MESSAGE_SIZE]) {
        // TODO add handling for completely failed connection
        if (!uah::areArraysEqual(message, ConnectionMessages::s_CONNECTION_REPEAT_MESSAGE)) {
            mpLogger->debug("Connection Method", "setLastTransmittedMessage(message)");
            setLastTransmittedMessage(message);
        }
        if (mpAddressing->getIsAddressingInProgress()) return;

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpRfModule->firstChangeRFChannel(mpAddressing->getConnectionRFChannel());

            createConnectionTimer();
            xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::endConnection() {
        mpLogger->debug("Connection Class", "Connection end.");
        if (mConnectionTimeoutTimer != nullptr) {
            xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
            deleteConnectionTimer();
        }

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mIsConnected = false;
        uah::clearBuffer(mConnectionFailedData.lastMessage, MESSAGE_SIZE);
        mConnectionFailedData.attempts = 0;
        xSemaphoreGive(mConnectionDataMutex);

        mpAddressing->setProtocolIPAddress(NULL_IP); // do anything only for Central Unit
        afterConnectionEndHandler();
    }

    void Connection::transmitRepeatMessage() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mIsConnected || mpAddressing->getIsAddressingInProgress()) {
            std::optional<API::CommandHandler> responseCommand;
            try {
                responseCommand.emplace(API::commandTypes::REPEAT);
            } catch (std::exception &e) {
                mpLogger->error("Connection transmitRepeatMessage", "Failed to create REPEAT command.");
                mpLogger->error("Connection messageDecider", e.what());
            }

            if (responseCommand.has_value()) {
                uint8_t buffer[MESSAGE_SIZE] = {};
                responseCommand->generateMessage(buffer);
                mpCommunication->sendPriorityMessage(buffer);
            }
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    // ================== Constructor and Destructor ==================
    Connection::Connection(
        Communication *communication,
#ifdef CENTRAL_UNIT
        const std::shared_ptr<CentralUnitAddressing> &addressing,
#else
        const std::shared_ptr<ModuleAddressing> &addressing,
#endif
        const std::shared_ptr<HC12> &rfModule,
        const std::shared_ptr<ul::Logger> &logger
    ) : mpCommunication(communication), mpAddressing(addressing), mpRfModule(rfModule), mpLogger(logger) {
        mConnectionDataMutex = xSemaphoreCreateMutex();

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        uah::clearBuffer(mConnectionFailedData.lastMessage, MESSAGE_SIZE);
        mConnectionFailedData.attempts = 0;
        xSemaphoreGive(mConnectionDataMutex);

        mpLogger->verbose("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        deleteConnectionTimer();
        vSemaphoreDelete(mConnectionDataMutex);
    }

    // ============================ Timers ============================
    void Connection::connectionTimerCallback(TimerHandle_t xTimer) {
        auto &con = *static_cast<Connection *>(pvTimerGetTimerID(xTimer));
        con.mpLogger->warning("Connection Class", "Connection timeout.");

        // if timeout occurred while changing channel
        xSemaphoreTake(con.mConnectionDataMutex, portMAX_DELAY);
        if (con.mTmpChannel.has_value()) {
            con.mTmpChannel.reset();
            xSemaphoreGive(con.mConnectionDataMutex);
            con.endConnection();
        } else {
            xSemaphoreGive(con.mConnectionDataMutex);
            con.repeatLastTransmittedMessage();
        }
    }

    void Connection::createConnectionTimer() {
        if (mConnectionTimeoutTimer == nullptr) {
            mConnectionTimeoutTimer = xTimerCreate(
                "Connection Timeout",
                pdMS_TO_TICKS(mConnectionFailedData.getTimeout()),
                pdFALSE,
                this,
                connectionTimerCallback
            );
        }
    }

    void Connection::deleteConnectionTimer() {
        if (mConnectionTimeoutTimer != nullptr) {
            xTimerDelete(mConnectionTimeoutTimer, portMAX_DELAY);
            mConnectionTimeoutTimer = nullptr;
        }
    }

    // ============================ Other =============================
    uint8_t Connection::getLastTransmittedMessageAttempts() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        const uint8_t result = mConnectionFailedData.attempts;
        xSemaphoreGive(mConnectionDataMutex);
        return result;
    }

    void Connection::setLastTransmittedMessage(const uint8_t message[MESSAGE_SIZE]) {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        uah::prepareBuffer(mConnectionFailedData.lastMessage, message, MESSAGE_SIZE, MESSAGE_SIZE);
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::repeatLastTransmittedMessage() {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mConnectionFailedData.lastMessage[0] != 0) {
            mpCommunication->sendPriorityMessage(mConnectionFailedData.lastMessage);
        }

        if (mConnectionFailedData.attempts > REPEAT_LAST_MESSAGE_MAX_ATTEMPTS) {
            xSemaphoreGive(mConnectionDataMutex);
            endConnection();
        } else {
            xTimerChangePeriod(mConnectionTimeoutTimer, mConnectionFailedData.getTimeout(), portMAX_DELAY);
            xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
            xSemaphoreGive(mConnectionDataMutex);
        }

        mConnectionFailedData.attempts++;
    }

    void Connection::afterConnectionEndHandler() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        // changing rf channel
#ifdef RF_CHANNELS
        mpRfModule->firstChangeRFChannel(mpAddressing->getDefaultRFChannel());
#else
#error "Not implemented"
#endif
        // going to sleep
        if (mSleepTime != 0) {
            auto &powerManager = ums::PowerManager::getInstance(mpLogger);
            powerManager.enterSleep(mSleepTime, !mIsDeepSleep);
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    uint16_t Connection::ConnectionFailedData::getTimeout() {
        const int32_t range = s_TIMEOUTS[attempts] * OFFSET_PERCENTAGE / 100;
        const int32_t offset = std::uniform_int_distribution<>(range * -1, range)(mGenerator);
        const uint16_t result = s_TIMEOUTS[attempts] + offset;
        return result;
    }
}
