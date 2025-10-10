#include "connection.h"

#include "communication/communication.h"
#include "universal_module_system/power_manager.h"

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

        // TODO before merge with main remove test messages
        // TODO consider ignoring 2 first letters in if statements (these letters are already checked)
        // TODO consider changing if else statements to switch case
        if (uah::areArraysEqual(receivedMessage, CONNECTION_END)) {
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_AFFIRM)) {
            uah::prepareBuffer(sendBuffer,CONNECTION_END, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_REPEAT_MESSAGE)) {
            repeatLastTransmittedMessage();
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_TEST_EXECUTE)) {
            uah::prepareBuffer(sendBuffer, CONNECTION_AFFIRM, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_TEST_GET)) {
            uah::prepareBuffer(sendBuffer, CONNECTION_RE_TEST_GET, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_RE_TEST_GET)) {
            uah::prepareBuffer(sendBuffer, CONNECTION_END, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_GO_TO_SLEEP_ONCE)) {
            // TODO !BEFORE PULL REQUEST! remove magic number
            xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
            mSleepTime = std::stoi((char*)&receivedMessage[7]);
            xSemaphoreGive(mConnectionDataMutex);
            uah::prepareBuffer(sendBuffer, CONNECTION_RE_GO_TO_SLEEP_ONCE, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_RE_GO_TO_SLEEP_ONCE)) {
            uah::prepareBuffer(sendBuffer, CONNECTION_END, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_GO_TO_SLEEP_PERIODICALLY)) {
            uah::prepareBuffer(sendBuffer, CONNECTION_RE_GO_TO_SLEEP_PERIODICALLY, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            // TODO !BEFORE PULL REQUEST! add
        } else if (uah::areArraysEqual(receivedMessage, CONNECTION_RE_GO_TO_SLEEP_PERIODICALLY)) {
            uah::prepareBuffer(sendBuffer, CONNECTION_END, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            endConnection();
        } else {
            mpLogger->warninga(
                "Connection Message Decider",
                "Received custom receivedMessage.\nIgnored receivedMessage: ",
                receivedMessage,
                MESSAGE_SIZE
            );
        }
    }

    void Connection::receivingHandle(const uint8_t ip) {
        if (mpAddressing->getIsAddressingInProgress()) return;

        createConnectionTimer();
        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            auto & powerManager = ums::PowerManager::getInstance(mpLogger);
            powerManager.disableAutoSleep();

            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpAddressing->setProtocolIPAddress(ip); // do anything only for Central Unit
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::sendingHandle(const uint8_t message[MESSAGE_SIZE]) {
        // TODO add handling for failed connection
        if (!uah::areArraysEqual(message, CONNECTION_REPEAT_MESSAGE)) {
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
        xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
        deleteConnectionTimer();
        mpLogger->debug("Connection Class", "Connection end.");

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mIsConnected = false;
        uah::clearBuffer(mLastTransmittedMessage, MESSAGE_SIZE);
        mLastTransmittedMessageAttempts = 0;
        xSemaphoreGive(mConnectionDataMutex);

        mpAddressing->setProtocolIPAddress(NULL_IP); // do anything only for Central Unit
        #ifdef RF_CHANNELS
            mpRfModule->firstChangeRFChannel(mpAddressing->getDefaultRFChannel());
        #else
            #error "Not implemented"
        #endif
        afterConnectionEndHandler();
    }

    void Connection::transmitRepeatMessage() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mIsConnected || mpAddressing->getIsAddressingInProgress()) {
            uint8_t buffer[MESSAGE_SIZE];
            uah::prepareBuffer(buffer, CONNECTION_REPEAT_MESSAGE, MESSAGE_SIZE);
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
        uah::clearBuffer(mLastTransmittedMessage, MESSAGE_SIZE);
        mLastTransmittedMessageAttempts = 0;
        xSemaphoreGive(mConnectionDataMutex);

        mpLogger->info("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        deleteConnectionTimer();
        vSemaphoreDelete(mConnectionDataMutex);
    }

    // ============================ Timers ============================
    constexpr uint16_t Connection::ms_TIMEOUTS[];

    void Connection::connectionTimerCallback(TimerHandle_t xTimer) {
        auto &con = *static_cast<Connection *>(pvTimerGetTimerID(xTimer));
        con.mpLogger->warning("Connection Class", "Connection timeout.");
        con.repeatLastTransmittedMessage();
    }

    void Connection::createConnectionTimer() {
        if (mConnectionTimeoutTimer == nullptr) {
            // TODO assign final values

            mConnectionTimeoutTimer = xTimerCreate(
                "Connection Timeout",
                pdMS_TO_TICKS(ms_TIMEOUTS[0]),
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
        const uint8_t result = mLastTransmittedMessageAttempts;
        xSemaphoreGive(mConnectionDataMutex);
        return result;
    }

    void Connection::setLastTransmittedMessage(const uint8_t message[MESSAGE_SIZE]) {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        uah::prepareBuffer(mLastTransmittedMessage, message, MESSAGE_SIZE, MESSAGE_SIZE);
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::repeatLastTransmittedMessage() {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mLastTransmittedMessage[0] != 0) {
            mpCommunication->sendPriorityMessage(mLastTransmittedMessage);
        }
        mLastTransmittedMessageAttempts++;

        if (mLastTransmittedMessageAttempts > REPEAT_LAST_MESSAGE_MAX_ATTEMPTS) {
            xSemaphoreGive(mConnectionDataMutex);
            endConnection();
        } else {
            xTimerChangePeriod(mConnectionTimeoutTimer, ms_TIMEOUTS[mLastTransmittedMessageAttempts - 1], portMAX_DELAY);
            xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
            xSemaphoreGive(mConnectionDataMutex);
        }
    }

    void Connection::afterConnectionEndHandler() {
        // TODO !BEFORE PULL REQUEST! add logic for sleep with and without rf module in sleep
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mSleepTime != 0) {
            auto & powerManager = ums::PowerManager::getInstance(mpLogger);
            powerManager.enterSleep(mSleepTime, true);
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

}