#pragma once

#include <memory>

#include "config/communication_config.h"

#include "utils/logger.h"
#include "utils/uint8_array_handlers.h"

namespace ul = Utils::Logging;
namespace uah = Utils::ArrayHandlers;

namespace Comms {
    class Communication;

    // TODO consider making this class singleton
    class Connection {
    public:
        static Connection& getInstance(Communication *communication, const std::shared_ptr<ul::Logger> &logger);

        // Delete copy constructor and assignment operator
        Connection(const Connection&) = delete;
        Connection& operator = (const Connection&) = delete;

        bool handleReceivedMessage(const uint8_t message[MESSAGE_SIZE]);
        void handleMessageToSend(const uint8_t message[MESSAGE_SIZE]);

    private:
        Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger);
        ~Connection();

        uint8_t getAckNumber() const;
        void setAckNumber(uint8_t number);
        uint8_t calculateNewAckNumber(const uint8_t *message, uint8_t lastAckNumber) const;

        bool getIsConnected() const;

        void sendConnectionRequest();

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

        TimerHandle_t mConnectionRequestTimeoutTimer = nullptr;
    };
}