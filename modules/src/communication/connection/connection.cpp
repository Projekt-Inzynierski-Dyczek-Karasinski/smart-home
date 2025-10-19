#include "connection.h"

#include "communication/communication.h"
#include "universal_module_system/power_manager/power_manager.h"

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

    void Connection::messageDecider(const uint8_t receivedMessage[MESSAGE_SIZE]) {
        uint8_t sendBuffer[MESSAGE_SIZE];

        using CM = ConnectionMessages;
        using CME = ConnectionMessages::MessagesEnum;
        // TODO !mm remove test messages
        // TODO consider ignoring 2 first letters in if statements (these letters are already checked)
        switch (
            const auto it = CM::messagesMap.find((char*)receivedMessage);
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

            case CME::TEST_GET:
                uah::prepareBuffer(sendBuffer,  CM::s_CONNECTION_RE_TEST_GET, MESSAGE_SIZE);
                mpCommunication->sendMessage(sendBuffer);
                break;

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
                    sleepTime = std::stoi((char*)&receivedMessage[strlen(CM::s_CONNECTION_GO_TO_NORMAL_SLEEP)]);
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
                    newChannel = std::stoi((char*)&receivedMessage[strlen(CM::s_CONNECTION_CHANGE_CHANNEL)]);
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

    void Connection::receivingHandle(const uint8_t ip) {
        if (mpAddressing->getIsAddressingInProgress()) return;

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        createConnectionTimer();
        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        if (!mIsConnected) {
            auto & powerManager = ums::PowerManager::getInstance(mpLogger);
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
        xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
        deleteConnectionTimer();

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
            uint8_t buffer[MESSAGE_SIZE];
            uah::prepareBuffer(buffer, ConnectionMessages::s_CONNECTION_REPEAT_MESSAGE, MESSAGE_SIZE);
            mpCommunication->sendPriorityMessage(buffer);
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

        mpLogger->info("Connection Class", "Connection initialized.");
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
            auto & powerManager = ums::PowerManager::getInstance(mpLogger);
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