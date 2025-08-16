#include "connection.h"

#include "smart_home_config.h"
#include "communication/communication.h"

// TODO !BEFORE PULL REQUEST! remove new instance of logger
Connection::Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
    : mpCommunication(communication), mpLogger(logger) {
    const auto tmpLogger = std::make_shared<ul::Logger>(ul::Level::DEBUG);
    mpLogger = tmpLogger;
}

void Connection::sendConnectionRequest(uint8_t *ackNumber) const {
    uint8_t buffer[MESSAGE_SIZE];
    *ackNumber = 1;
    buffer[0] = *ackNumber;
    uah::clearBuffer(&buffer[1], MESSAGE_SIZE - 1);

    mpCommunication->sendMessage(buffer);
}

void Connection::createConnectionTask(const TaskFunction_t task) {
    if (mConnectionTaskHandle == nullptr) {
        xTaskCreate(
            task,
            "Connection Task",
            2048,
            nullptr,
            LOW_TASK_PRIORITY,
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
