#include "connection.h"

#include "smart_home_config.h"
#include "communication/communication.h"

Connection *Connection::mspConnection = nullptr;

// TODO !BEFORE PULL REQUEST! remove new instance of logger
Connection::Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
    : mpCommunication(communication), mpLogger(logger) {
    mAckNumberMutex = xSemaphoreCreateMutex();

    mspConnection = this;
    const auto tmpLogger = std::make_shared<ul::Logger>(ul::Level::DEBUG);
    mpLogger = tmpLogger;

    createConnectionQueues();
    createConnectionTimers();
}

Connection::~Connection() {
    deleteConnectionTask();
    deleteConnectionTimers();
    deleteConnectionQueues();
}

// TODO add suspending and resuming connection task
void Connection::handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]) const {
    xQueueSend(mReceiveQueue, message, portMAX_DELAY);
}

void Connection::handleMessageToSend(const uint8_t message[MESSAGE_SIZE]) const {
    xQueueSend(mSendQueue, message, portMAX_DELAY);
}

// TODO consider changing length of queues to 1
void Connection::createConnectionQueues() {
    if (mReceiveQueue == nullptr) {
        mReceiveQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
    if (mSendQueue == nullptr) {
        mSendQueue = xQueueCreate(MESSAGE_QUEUE_LEN, sizeof(uint8_t[MESSAGE_SIZE]));
    }
}

void Connection::deleteConnectionQueues() {
    if (mReceiveQueue != nullptr) {
        vQueueDelete(mReceiveQueue);
        mReceiveQueue = nullptr;
    }
    if (mSendQueue != nullptr) {
        vQueueDelete(mSendQueue);
        mSendQueue = nullptr;
    }
}

uint8_t Connection::getAckNumber() const {
    xSemaphoreTake(mAckNumberMutex, portMAX_DELAY);
    const uint8_t result = mAckNumber;
    xSemaphoreGive(mAckNumberMutex);
    return result;
}

void Connection::setAckNumber(const uint8_t number) {
    xSemaphoreTake(mAckNumberMutex, portMAX_DELAY);
    mAckNumber = number;
    xSemaphoreGive(mAckNumberMutex);
}


void Connection::sendConnectionRequest() {
    uint8_t buffer[MESSAGE_SIZE];
    buffer[0] = START_ACK_NUMBER;
    uah::clearBuffer(&buffer[1], MESSAGE_SIZE - 1);
    setAckNumber(START_ACK_NUMBER);

    mpCommunication->sendMessage(buffer);
}


void Connection::createConnectionTask(const TaskFunction_t task) {
    if (mConnectionTaskHandle == nullptr) {
        xTaskCreate(
            task,
            "Connection Task",
            2048,
            nullptr,
            MEDIUM_TASK_PRIORITY,
            &mConnectionTaskHandle
        );
    } else {
        mpLogger->warning("Connection FreeRTOS", "Can't create Connection task, because task already exists.");
    }
}

void Connection::deleteConnectionTask() {
    if (mConnectionTaskHandle != nullptr) {
        vTaskDelete(mConnectionTaskHandle);
        mConnectionTaskHandle = nullptr;
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