#include "connection.h"

#include "communication/communication.h"

// #include "communication/addressing/central_unit_addressing.h"

namespace Comms {
    Connection *Connection::mspConnection = nullptr;

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
        // TODO !BEFORE PULL REQUEST! remove new instance of logger
        // const auto tmpLogger = std::make_shared<ul::Logger>(ul::Level::DEBUG);
        // mpLogger = tmpLogger;
        createConnectionTimers();
        // createConnectionQueues();
        // createConnectionTask();
        mpLogger->info("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        // deleteConnectionTask();
        // deleteConnectionQueues();
        deleteConnectionTimers();
        vSemaphoreDelete(mConnectionDataMutex);
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
        if (mpAddressing->getIsAddressingWorking()) return;

        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpRfModule->changeRFChannel(mpAddressing->getConnectionRFChannel());
        }
        xSemaphoreGive(mConnectionDataMutex);
        mpLogger->debug("Connection Class", "tmp.");
    }

    void Connection::endConnection() {
        xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
        mpLogger->debug("Connection Class", "Connection end.");

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mIsConnected = false;
        // mAckNumber = START_ACK_NUMBER;
        // mIsPossibleEndOfConnection = false;
        // mConnectionStatus = ConnectionStatus::DISCONNECTED;
        xSemaphoreGive(mConnectionDataMutex);

        mpAddressing->setProtocolIPAddress(NULL_IP); // do anything only for Central Unit
        #ifdef RF_CHANNELS
            mpRfModule->changeRFChannel(mpAddressing->getDefaultRFChannel());
        #else
           #error "Not implemented"
        #endif
    }

    // TODO remove if not used
    void Connection::startConnection() {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mIsConnected = true;
        xSemaphoreGive(mConnectionDataMutex);
    }

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
}
