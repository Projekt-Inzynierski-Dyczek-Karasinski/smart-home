#include "connection.h"

#include "smart_home_config.h"
#include "communication/communication.h"

namespace Comms {
    Connection *Connection::mspConnection = nullptr;

    Connection &Connection::getInstance(Communication *communication, const std::shared_ptr<ul::Logger> &logger) {
        static Connection instance(communication, logger);
        return instance;
    }

    // TODO !BEFORE PULL REQUEST! remove new instance of logger
    Connection::Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : mpCommunication(communication), mpLogger(logger) {
        mConnectionDataMutex = xSemaphoreCreateMutex();
        mTransmittingSemaphore = xSemaphoreCreateBinary();

        mspConnection = this;
        const auto tmpLogger = std::make_shared<ul::Logger>(ul::Level::DEBUG);
        mpLogger = tmpLogger;

        createConnectionTimers();

        mpLogger->info("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        // deleteConnectionTask();
        deleteConnectionTimers();
        // deleteConnectionQueues();
        vSemaphoreDelete(mConnectionDataMutex);
        vSemaphoreDelete(mTransmittingSemaphore);
    }

    bool Connection::handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]) {
        // if is special message
        if (message[ACK_NUMBER_INDEX] == 0) {
            return true;
        }

        bool isAckNumberCorrect = false;
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (message[ACK_NUMBER_INDEX] == mAckNumber) {
            isAckNumberCorrect = true;
            mAckNumber = calculateNewAckNumber(message, mAckNumber);
        }
        xSemaphoreGive(mConnectionDataMutex);

        // signalize that now can transmit again
        xSemaphoreGive(mTransmittingSemaphore);
        return isAckNumberCorrect;
    }

    void Connection::handleMessageToSend(const uint8_t message[MESSAGE_SIZE]) {
        
        xSemaphoreTake(mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_REQUEST_TIMEOUT));
        // if is special message
        if (message[ACK_NUMBER_INDEX] == 0) {

        } else if (getIsConnected()) {
            // wait for mTransmittingSemaphore and send data, but add first ack number
            // if fails send last transmitted message
        } else {
            // start timeout timer, send new connection request and when approved send data
        }
        // xQueueSend(mSendQueue, message, portMAX_DELAY);
    }

    uint8_t Connection::getAckNumber() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        const uint8_t result = mAckNumber;
        xSemaphoreGive(mConnectionDataMutex);
        return result;
    }

    void Connection::setAckNumber(const uint8_t number) {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mAckNumber = number;
        xSemaphoreGive(mConnectionDataMutex);
    }

    uint8_t Connection::calculateNewAckNumber(const uint8_t *message, const uint8_t lastAckNumber) const {
        uint8_t index = 0;
        uint16_t result = lastAckNumber;
        while (message[index] != 0) {
            result += message[index];
            index++;
        }
        result = (result % 254) + 1;
        return (uint8_t)result;
    }

    bool Connection::getIsConnected() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        const bool result = mIsConnected;
        xSemaphoreGive(mConnectionDataMutex);
        return result;
    }

    void Connection::sendConnectionRequest() {
        uint8_t buffer[MESSAGE_SIZE];
        buffer[0] = START_ACK_NUMBER;
        uah::clearBuffer(&buffer[1], MESSAGE_SIZE - 1);
        setAckNumber(START_ACK_NUMBER);

        mpCommunication->sendMessage(buffer);
    }

    void Connection::connectionTimersCallbacks(const TimerHandle_t xTimer) {
        auto &con = *mspConnection;
        if (xTimer == con.mConnectionRequestTimeoutTimer) {
            // TODO add max attempts
            con.sendConnectionRequest();
        }
    }

    void Connection::createConnectionTimers() {
        if (mConnectionRequestTimeoutTimer == nullptr) {
            mConnectionRequestTimeoutTimer = xTimerCreate(
                "Con Req Timeout",
                pdMS_TO_TICKS(CONNECTION_REQUEST_TIMEOUT),
                pdTRUE,
                nullptr,
                connectionTimersCallbacks
            );
        }
    }

    void Connection::deleteConnectionTimers() {
        if (mConnectionRequestTimeoutTimer != nullptr) {
            xTimerDelete(mConnectionRequestTimeoutTimer, portMAX_DELAY);
            mConnectionRequestTimeoutTimer = nullptr;
        }
    }
}