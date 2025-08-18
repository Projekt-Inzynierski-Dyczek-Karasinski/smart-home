#pragma once

#include <memory>

#include "config/communication_config.h"

#include "utils/logger.h"
#include "utils/uint8_array_handlers.h"

namespace ul = Utils::Logging;
namespace uah = Utils::ArrayHandlers;

class Communication;

class Connection {
public:
    Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger);

    virtual ~Connection();

    void handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]) const;
    void handleMessageToSend(const uint8_t message[MESSAGE_SIZE]) const;

protected:
    void createConnectionQueues();
    void deleteConnectionQueues();

    uint8_t getAckNumber() const;
    void setAckNumber(uint8_t number);

    void sendConnectionRequest();

    void createConnectionTask(TaskFunction_t task);
    void deleteConnectionTask();

    static void connectionTimersCallbacks(TimerHandle_t xTimer);
    void createConnectionTimers();
    void deleteConnectionTimers();

    static Connection *mspConnection;
    Communication *mpCommunication; ///< Pointer to the Communication class instance.
    std::shared_ptr<ul::Logger> mpLogger;

    uint8_t mAckNumber = START_ACK_NUMBER;

    xSemaphoreHandle mAckNumberMutex = nullptr;

    QueueHandle_t mReceiveQueue = nullptr;
    QueueHandle_t mSendQueue = nullptr;

    TaskHandle_t mConnectionTaskHandle = nullptr;

    TimerHandle_t mConnectionRequestTimeoutTimer = nullptr;
};


