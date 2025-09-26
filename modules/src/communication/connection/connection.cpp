#include "connection.h"

#include "communication/communication.h"

namespace Comms {
    Connection *Connection::mspConnection = nullptr;

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
        if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_END, SPECIAL_MESSAGE_LEN)) {
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_AFFIRM, SPECIAL_MESSAGE_LEN)) {
            uah::prepareBuffer(sendBuffer, (uint8_t*)CONNECTION_END, 5, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN)) {
            repeatLastTransmittedMessage();
        } else if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_TEST_EXECUTE, 5)) {
            uah::prepareBuffer(sendBuffer, (uint8_t*)CONNECTION_AFFIRM, 4, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
        } else if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_TEST_GET, 6)) {
            uah::prepareBuffer(sendBuffer, (uint8_t*)CONNECTION_RE_TEST_GET, 26, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
        } else if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_RE_TEST_GET, 26)) {
            uah::prepareBuffer(sendBuffer, (uint8_t*)CONNECTION_END, 5, MESSAGE_SIZE);
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
        if (mpAddressing->getIsAddressingWorking()) return;

        createConnectionTimer();
        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpAddressing->setProtocolIPAddress(ip); // do anything only for Central Unit
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::sendingHandle(const uint8_t message[MESSAGE_SIZE]) {
        // TODO add handling for failed connection
        if (!uah::areArraysEqual(message, (uint8_t*)CONNECTION_REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN)) {
            mpLogger->debug("Connection Method", "setLastTransmittedMessage(message)");
            setLastTransmittedMessage(message);
        }
        if (mpAddressing->getIsAddressingWorking()) return;

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
    }

    void Connection::transmitRepeatMessage() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mIsConnected || mpAddressing->getIsAddressingWorking()) {
            uint8_t buffer[MESSAGE_SIZE];
            uah::prepareBuffer(buffer, (uint8_t*)CONNECTION_REPEAT_MESSAGE, SPECIAL_MESSAGE_LEN, MESSAGE_SIZE);
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
        mspConnection = this;
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
    void Connection::connectionTimerCallback(TimerHandle_t xTimer) {
        auto &con = *mspConnection;
        // TODO remove if?
        if (xTimer == con.mConnectionTimeoutTimer) {
            con.mpLogger->error("Connection Class", "Connection timeout.");
            con.endConnection();
        }
    }

    void Connection::createConnectionTimer() {
        if (mConnectionTimeoutTimer == nullptr) {
            mConnectionTimeoutTimer = xTimerCreate(
                "Connection Timeout",
                pdMS_TO_TICKS(CONNECTION_TIMEOUT),
                pdFALSE,
                nullptr,
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
        xSemaphoreGive(mConnectionDataMutex);

        // TODO consider better handling that:
        if (mLastTransmittedMessageAttempts > REPEAT_LAST_MESSAGE_MAX_ATTEMPTS) {
            endConnection();
        }
    }
}