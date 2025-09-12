#pragma once

#include <memory>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "utils/logger.h"
#include "utils/uint8_array_handlers.h"

namespace ul = Utils::Logging;
namespace uah = Utils::ArrayHandlers;

namespace Comms {
    class Communication;
    class Addressing;
    #ifdef HC12_MODULE
        class HC12;
    #else
        #error "Not implemented"
    #endif

    class Connection {
    public:
        static Connection& getInstance(Communication *communication, const std::shared_ptr<Addressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger);

        // Delete copy constructor and assignment operator
        Connection(const Connection&) = delete;
        Connection& operator = (const Connection&) = delete;

        void sendMessage(const uint8_t message[MESSAGE_SIZE]) const;

        void suspendConnectionTask() const;

        bool handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]);
        void handleMessageToSend(const uint8_t message[MESSAGE_SIZE]);
        void endConnection();
    private:
        Connection(Communication *communication, const std::shared_ptr<Addressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger);

        ~Connection();

        uint8_t getAckNumber() const;
        void setAckNumber(uint8_t number);
        uint8_t calculateNewAckNumber(const uint8_t *message, uint8_t lastAckNumber) const;

        bool getIsConnected() const;

        void sendConnectionRequest();

        void createConnectionQueues();
        void deleteConnectionQueues();


        bool handleNewConnection();
        static void connectionTask(void *parameters);
        void createConnectionTask();
        void deleteConnectionTask();

        static void connectionTimersCallbacks(TimerHandle_t xTimer);
        void createConnectionTimers();
        void deleteConnectionTimers();

        static Connection *mspConnection;
        Communication *mpCommunication; ///< Pointer to the Communication class instance.
        std::shared_ptr<Addressing> mpAddressing;
        #ifdef HC12_MODULE
            std::shared_ptr<HC12> mpRfModule;
        #else
            #error "Not implemented"
        #endif
        std::shared_ptr<ul::Logger> mpLogger;

        uint8_t mAckNumber = START_ACK_NUMBER;
        bool mIsConnected = false;
        bool mIsPossibleEndOfConnection = false; // TODO change name of this var

        xSemaphoreHandle mConnectionDataMutex = nullptr;
        xSemaphoreHandle mTransmittingSemaphore = nullptr;

        TaskHandle_t mConnectionTask = nullptr;
        // TODO !BEFORE PULL REQUEST! update comment
        QueueHandle_t mSendMessagesQueue = nullptr; ///< Handle to FreeRTOS queue for messages to encode and RF transmission, queue length: 10x64 bytes (uint8_t).

        TimerHandle_t mConnectionRequestTimeoutTimer = nullptr;
    };
}