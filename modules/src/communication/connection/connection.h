#pragma once

#include <memory>

#include "config/communication_config.h"

#include "utils/logger.h"
#include "utils/uint8_array_handlers.h"

namespace ul = Utils::Logging;
namespace uah = Utils::ArrayHandlers;

class Communication;

// TODO consider making this class singleton
class Connection {
public:
    Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger);

    virtual ~Connection();

    bool handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]);
    void handleMessageToSend(const uint8_t message[MESSAGE_SIZE]);

protected:
    // void createConnectionQueues();
    // void deleteConnectionQueues();

    uint8_t getAckNumber() const;
    void setAckNumber(uint8_t number);
    bool isAckNumberCorrect(uint8_t ackNumber);

    bool getIsConnected() const;

    void sendConnectionRequest();

    // void createConnectionTask(TaskFunction_t task);
    // void deleteConnectionTask();

    static void connectionTimersCallbacks(TimerHandle_t xTimer);
    void createConnectionTimers();
    void deleteConnectionTimers();

    static Connection *mspConnection;
    Communication *mpCommunication; ///< Pointer to the Communication class instance.
    std::shared_ptr<ul::Logger> mpLogger;

    uint8_t mAckNumber = START_ACK_NUMBER;
    bool mIsConnected = false;
    bool mIsPossibleEndOfConnection = false; // TODO change name of this var

    xSemaphoreHandle mConnectionDataMutex = nullptr;
    xSemaphoreHandle mTransmittingSemaphore = nullptr;

    // QueueHandle_t mReceiveQueue = nullptr;
    // QueueHandle_t mSendQueue = nullptr;

    // TaskHandle_t mConnectionTaskHandle = nullptr;

    TimerHandle_t mConnectionRequestTimeoutTimer = nullptr;
};


