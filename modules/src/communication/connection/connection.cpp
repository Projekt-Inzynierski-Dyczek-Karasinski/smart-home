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

        // TODO consider changing if else statements to switch case
        // TODO before merge with main remove test messages
        if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_END, 5)) {
            endConnection();
        } else if (uah::areArraysEqual(receivedMessage, (uint8_t*)CONNECTION_AFFIRM, 4)) {
            uah::prepareBuffer(sendBuffer, (uint8_t*)CONNECTION_END, 5, MESSAGE_SIZE);
            mpCommunication->sendMessage(sendBuffer);
            endConnection();
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

        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpAddressing->setProtocolIPAddress(ip); // do anything only for Central Unit
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::sendingHandle() {
        // TODO add handling for failed connection
        if (mpAddressing->getIsAddressingWorking()) return;

        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpRfModule->firstChangeRFChannel(mpAddressing->getConnectionRFChannel());
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
        mspConnection = this;
        // TODO consider creating timer only when is needed
        createConnectionTimers();
        mpLogger->info("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        deleteConnectionTimers();
        vSemaphoreDelete(mConnectionDataMutex);
    }

    // ============================ Timers ============================
    void Connection::connectionTimersCallbacks(TimerHandle_t xTimer) {
        auto &con = *mspConnection;
        if (xTimer == con.mConnectionTimeoutTimer) {
            con.mpLogger->error("Connection Class", "Connection timeout.");
            con.endConnection();
        }
    }

    void Connection::createConnectionTimers() {
        if (mConnectionTimeoutTimer == nullptr) {
            mConnectionTimeoutTimer = xTimerCreate(
                "Connection Timeout",
                pdMS_TO_TICKS(CONNECTION_TIMEOUT),
                pdTRUE,
                nullptr,
                connectionTimersCallbacks
            );
        }
    }

    void Connection::deleteConnectionTimers() {
        if (mConnectionTimeoutTimer != nullptr) {
            xTimerDelete(mConnectionTimeoutTimer, portMAX_DELAY);
            mConnectionTimeoutTimer = nullptr;
        }
    }

    // ============================ Other =============================
    void Connection::endConnection() {
        xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
        mpLogger->debug("Connection Class", "Connection end.");

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mIsConnected = false;
        xSemaphoreGive(mConnectionDataMutex);

        mpAddressing->setProtocolIPAddress(NULL_IP); // do anything only for Central Unit
        #ifdef RF_CHANNELS
            mpRfModule->firstChangeRFChannel(mpAddressing->getDefaultRFChannel());
        #else
            #error "Not implemented"
        #endif
    }
}