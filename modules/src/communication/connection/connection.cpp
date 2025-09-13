#include "connection.h"

#include "communication/communication.h"
#include "communication/addressing/central_unit_addressing.h"

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
    ): mpCommunication(communication), mpAddressing(addressing), mpRfModule(rfModule), mpLogger(logger) {
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
        mpLogger->debug("Connection FreeRTOS", "vTaskSuspend(mConnectionTask);");
        vTaskSuspend(mConnectionTask);
    }

    bool Connection::handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]) {
        // if is special message
        if (message[ACK_NUMBER_INDEX] == SPECIAL_ACK_NUMBER) {
            // TODO add check for "repeat" message and move methods for "repeat" message to this class
            return true;
        }

        bool isAckNumberCorrect = false;
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mConnectionStatus == ConnectionStatus::CONNECTED) {
            if (message[ACK_NUMBER_INDEX] == mAckNumber) {
                isAckNumberCorrect = true;
                mAckNumber = calculateNewAckNumber(message, mAckNumber);
                if (uah::calcLenOfDataInArray(message, 2) == 1) {
                    if (mIsPossibleEndOfConnection) {
                        endConnection();
                    } else {
                        mIsPossibleEndOfConnection = true;
                    }
                }
                xSemaphoreGive(mTransmittingSemaphore); // signalize that now can transmit
            }
        } else {
            // if it is single char message
            if (uah::calcLenOfDataInArray(message, 2) == 1) {
                // if it is not connected and get new connection request
                if (mConnectionStatus == ConnectionStatus::DISCONNECTED && message[ACK_NUMBER_INDEX] == START_ACK_NUMBER) {
                    mpLogger->debug("Connection Method", "Get new connection request.");
                    mConnectionStatus = ConnectionStatus::CONNECTED;
                    mAckNumber = calculateNewAckNumber(message, mAckNumber);
                    uint8_t messageBuffer[MESSAGE_SIZE];
                    messageBuffer[ACK_NUMBER_INDEX] = mAckNumber;
                    uah::clearBuffer(&messageBuffer[1], MESSAGE_SIZE - 1);

                    // TODO remove
                    mpLogger->debugv("Connection Method", "Sending mAckNumber: ", mAckNumber);

                    mpCommunication->encodeMessage(messageBuffer); // send response to connection request
                    mAckNumber = calculateNewAckNumber(message, mAckNumber);
                    // TODO remove
                    mpLogger->debugv("Connection Method", "Expected mAckNumber: ", mAckNumber);
                }
                // if it is trying to connect and get response to new connection request
                else if (mConnectionStatus == ConnectionStatus::TRYING_TO_CONNECT && message[ACK_NUMBER_INDEX] == START_ACK_NUMBER + 1) {
                    mpLogger->debug("Connection Method", "Get response to new connection request.");
                    isAckNumberCorrect = true;
                    mConnectionStatus = ConnectionStatus::CONNECTED;
                    mAckNumber = calculateNewAckNumber(message, mAckNumber);

                    // TODO remove
                    mpLogger->debugv("Connection Method", "TRYING_TO_CONNECT mAckNumber: ", mAckNumber);

                    xSemaphoreGive(mTransmittingSemaphore); // signalize that now can transmit
                }
            }
        }
        xSemaphoreGive(mConnectionDataMutex);

        return isAckNumberCorrect;
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
        result = result % 255;
        return (uint8_t)result;
    }

    void Connection::sendConnectionRequest() {
        uint8_t buffer[MESSAGE_SIZE];
        buffer[ACK_NUMBER_INDEX] = START_ACK_NUMBER;
        uah::clearBuffer(&buffer[1], MESSAGE_SIZE - 1);
        setAckNumber(START_ACK_NUMBER);

        mpCommunication->sendMessage(buffer);
    }

    void Connection::endConnection() {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mAckNumber = START_ACK_NUMBER;
        mIsPossibleEndOfConnection = false;
        mConnectionStatus = ConnectionStatus::DISCONNECTED;
        xSemaphoreGive(mConnectionDataMutex);

        mpAddressing->setProtocolIPAddress(CENTRAL_UNIT_IP); // do anything only for Central Unit
        #ifdef RF_CHANNELS
            mpRfModule->changeRFChannel(mpAddressing->getDefaultRFChannel());
        #endif
        xSemaphoreGive(mTransmittingSemaphore);
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
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mConnectionStatus == ConnectionStatus::CONNECTED) {
            xSemaphoreGive(mConnectionDataMutex);
            return true;
        }
        mConnectionStatus = ConnectionStatus::TRYING_TO_CONNECT;
        xSemaphoreGive(mConnectionDataMutex);

        // TODO add increasing delay
        uint8_t messageBuffer[MESSAGE_SIZE];
        constexpr uint8_t message[] = {START_ACK_NUMBER};
        #ifdef RF_CHANNELS

            mpRfModule->changeRFChannel(mpAddressing->getConnectionRFChannel());
        #endif
        uah::prepareBuffer(messageBuffer, message, 1, MESSAGE_SIZE);
        for (uint8_t attempt = 0; attempt < CONNECTION_MAX_ATTEMPTS; attempt++) {
            xSemaphoreTake(mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT));
            xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
            // WARNING: CLion information "Condition is always false" is NOT true
            if (mConnectionStatus == ConnectionStatus::CONNECTED) {
                xSemaphoreGive(mConnectionDataMutex);
                xSemaphoreGive(mTransmittingSemaphore);

                // TODO remove
                mpLogger->debug("Connection handleNewConnection()", "Connected");

                return true;
            }

            xSemaphoreGive(mConnectionDataMutex);
            mpCommunication->encodeMessage(messageBuffer);

        }
        return false;

        // uint8_t attemptCounter = 0;
        // uint8_t messageBuffer[MESSAGE_SIZE];
        // constexpr uint8_t message[] = {START_ACK_NUMBER};
        // while (!getIsConnected() && attemptCounter < CONNECTION_MAX_ATTEMPTS) {
        //     if (attemptCounter == 0) {
        //         mpAddressing->getConnectionRFChannel();
        //         uah::prepareBuffer(messageBuffer, message, 1, MESSAGE_SIZE);
        //     }
        //     attemptCounter++;
        //     setAckNumber(START_ACK_NUMBER);
        //     mpCommunication->encodeMessage(messageBuffer);
        //     xSemaphoreTake(mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT));
        //     mpLogger->warning("Connection Task", "New connection attempt failed.");
        // }
        // return attemptCounter >= CONNECTION_MAX_ATTEMPTS;
    }

    void Connection::connectionTask(void *parameters) {
        auto &con = *mspConnection;
        uint8_t messageBuffer[MESSAGE_SIZE];
        uah::clearBuffer(messageBuffer, MESSAGE_SIZE);

        for (;;) {
            // TODO change xTicksToWait ?
            if (xQueueReceive(con.mSendMessagesQueue, messageBuffer, pdMS_TO_TICKS(CONNECTION_TIMEOUT)) == pdTRUE) {
                // special messages
                if (messageBuffer[ACK_NUMBER_INDEX] == SPECIAL_ACK_NUMBER) {
                    con.mpLogger->debuga("Connection Task", "Sending special message: ", messageBuffer, MESSAGE_SIZE);
                    con.mpCommunication->encodeMessage(messageBuffer);
                    continue;
                }
                
                if (!con.handleNewConnection()) {
                    con.mpLogger->error("Connection Task", "Exited max number of connection attempts.");
                    // xSemaphoreGive(con.mTransmittingSemaphore);
                    con.endConnection();
                    continue;
                }

                // wait for semaphore (received RF message), calc and add ack num and call con.mpCommunication->encodeMessage(messageBuffer);
                if (xSemaphoreTake(con.mTransmittingSemaphore, pdMS_TO_TICKS(CONNECTION_TIMEOUT)) == pdFALSE) {
                    con.mpLogger->error("Connection Task", "Connection loss.");
                    con.endConnection();
                    continue;
                }

                // TODO consider changing that
                xSemaphoreTake(con.mConnectionDataMutex, portMAX_DELAY);
                messageBuffer[ACK_NUMBER_INDEX] = con.mAckNumber;
                xSemaphoreGive(con.mConnectionDataMutex);

                con.mpCommunication->encodeMessage(messageBuffer);
            } else {
                con.endConnection();
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
