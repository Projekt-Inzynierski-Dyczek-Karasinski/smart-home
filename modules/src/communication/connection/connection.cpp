#include "connection.h"

#include "communication/communication.h"

namespace Comms {
    Connection *Connection::mspConnection = nullptr;

    Connection &Connection::getInstance(Communication *communication, const std::shared_ptr<Addressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger) {
        static Connection instance(communication, addressing, rfModule, logger);
        return instance;
    }

    Connection::Connection(Communication *communication, const std::shared_ptr<Addressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger)
        : mpCommunication(communication), mpAddressing(addressing), mpRfModule(rfModule), mpLogger(logger) {
        mConnectionDataMutex = xSemaphoreCreateMutex();
        mTransmittingSemaphore = xSemaphoreCreateBinary();

        mspConnection = this;
        // TODO !BEFORE PULL REQUEST! remove new instance of logger
        const auto tmpLogger = std::make_shared<ul::Logger>(ul::Level::DEBUG);
        mpLogger = tmpLogger;

        createConnectionTimers();
        createConnectionQueues();
        createConnectionTask();

        mpLogger->info("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        deleteConnectionTask();
        deleteConnectionQueues();
        deleteConnectionTimers();
        vSemaphoreDelete(mConnectionDataMutex);
        vSemaphoreDelete(mTransmittingSemaphore);
    }

    void Connection::sendMessage(const uint8_t message[MESSAGE_SIZE]) const {
        xQueueSend(mSendMessagesQueue, message, portMAX_DELAY);
        vTaskResume(mConnectionTask);
    }

    void Connection::suspendConnectionTask() const {
        mpLogger->debug("Connection Class", "vTaskSuspend(mConnectionTask);");
        vTaskSuspend(mConnectionTask);
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
        
        xSemaphoreTake(mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT));
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
        result = (result % 255) + 1;
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

    void Connection::endConnection() {
        mpAddressing->setProtocolIPAddress(CENTRAL_UNIT_IP);
        #ifdef RF_CHANNELS
                mpRfModule->changeRFChannel(mpAddressing->getDefaultRFChannel());
        #endif
    }

    void Connection::createConnectionQueues() {
        if (mSendMessagesQueue == nullptr) {
            mSendMessagesQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
        }
    }
    void Connection::deleteConnectionQueues() {
        if (mSendMessagesQueue != nullptr) {
            vQueueDelete(mSendMessagesQueue);
            mSendMessagesQueue = nullptr;
        }
    }

    bool Connection::handleNewConnection() {
        // TODO add increasing delay
        uint8_t attemptCounter = 0;
        uint8_t messageBuffer[MESSAGE_SIZE];
        constexpr uint8_t message[] = {START_ACK_NUMBER};
        while (!getIsConnected() && attemptCounter < CONNECTION_MAX_ATTEMPTS) {
            if (attemptCounter == 0) {
                mpAddressing->getConnectionRFChannel();
                uah::prepareBuffer(messageBuffer, message, 1, MESSAGE_SIZE);
            }
            attemptCounter++;
            setAckNumber(START_ACK_NUMBER);
            mpCommunication->encodeMessage(messageBuffer);
            xSemaphoreTake(mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT));
            mpLogger->warning("Connection Task", "New connection attempt failed.");
        }
        return attemptCounter >= CONNECTION_MAX_ATTEMPTS;
    }

    void Connection::connectionTask(void *parameters) {
        auto &con = *mspConnection;
        uint8_t messageBuffer[MESSAGE_SIZE];
        uah::clearBuffer(messageBuffer, MESSAGE_SIZE);
        con.mIsConnected = false;

        for (;;) {
            if (xQueueReceive(con.mSendMessagesQueue, messageBuffer, pdMS_TO_TICKS(SUSPEND_TASK_TIME_LONG)) == pdTRUE) {
                // special messages
                if (messageBuffer[ACK_NUMBER_INDEX] == 0) {
                    con.mpLogger->debuga("Connection Task", "Sending special message: ", messageBuffer, MESSAGE_SIZE);
                    con.mpCommunication->encodeMessage(messageBuffer);
                    continue;
                }
                
                if (!con.handleNewConnection()) {
                    con.mpLogger->error("Connection Task", "Exited max number of connection attempts.");
                    // TODO add handler for failed connection
                    xSemaphoreGive(con.mTransmittingSemaphore);
                    continue;
                }

                // wait for semaphore (received RF message), calc and add ack num and call con.mpCommunication->encodeMessage(messageBuffer);
                if (xSemaphoreTake(con.mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT)) == pdTRUE) {

                } else {
                    // TODO add handler for failed connection
                    con.mpLogger->error("Connection Task", "Connection loss.");
                }
                con.mpCommunication->encodeMessage(messageBuffer);

            } else {
                con.mpCommunication->suspendConnectionTask();
            }
        }
    }

    void Connection::createConnectionTask() {
        if (mConnectionTask == nullptr) {
            xTaskCreate(
                connectionTask,
                "Connection Task",
                2048,
                nullptr,
                LOW_TASK_PRIORITY,
                &mConnectionTask
            );
        } else {
            mpLogger->warning("Connection FreeRTOS", "Can't create Connection task, because task already exists.");
        }
    }
    void Connection::deleteConnectionTask() {
        endConnection();
        if (mConnectionTask != nullptr) {
            vTaskDelete(mConnectionTask);
            mConnectionTask = nullptr;
        }
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
                pdMS_TO_TICKS(CONNECTION_TIMEOUT),
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