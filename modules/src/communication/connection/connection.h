#pragma once

#include <memory>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "utils/logger.h"
#include "utils/uint8_array_handlers.h"

#ifdef CENTRAL_UNIT
    #include "communication/addressing/central_unit_addressing.h"
#else
    #include "communication/addressing/module_addressing.h"
#endif


namespace ul = Utils::Logging;
namespace uah = Utils::ArrayHandlers;

namespace Comms {
    class Communication;

    #ifdef HC12_MODULE
        class HC12;
    #else
        #error "Not implemented"
    #endif

    // TODO !BEFORE PULL REQUEST! add comments
    class Connection {
    public:
        #ifdef CENTRAL_UNIT
            static Connection& getInstance(Communication *communication, const std::shared_ptr<CentralUnitAddressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger);
        #else
            static Connection& getInstance(Communication *communication, const std::shared_ptr<ModuleAddressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger);
        #endif

        // Delete copy constructor and assignment operator
        Connection(const Connection&) = delete;
        Connection& operator = (const Connection&) = delete;

        void messageDecider(const uint8_t receivedMessage[MESSAGE_SIZE]);

        void receivingHandle(uint8_t ip);
        void sendingHandle();

    private:
        #ifdef CENTRAL_UNIT
            Connection(Communication *communication, const std::shared_ptr<CentralUnitAddressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger);
        #else
            Connection(Communication *communication, const std::shared_ptr<ModuleAddressing> &addressing, const std::shared_ptr<HC12> &rfModule, const std::shared_ptr<ul::Logger> &logger);
        #endif
        ~Connection();

        static void connectionTimersCallbacks(TimerHandle_t xTimer);
        void createConnectionTimers();
        void deleteConnectionTimers();

        void endConnection();

        static Connection *mspConnection;
        Communication *mpCommunication; ///< Pointer to the Communication class instance.

        #ifdef CENTRAL_UNIT
            std::shared_ptr<CentralUnitAddressing> mpAddressing;
        #else
            std::shared_ptr<ModuleAddressing> mpAddressing;
        #endif

        #ifdef HC12_MODULE
            std::shared_ptr<HC12> mpRfModule;
        #else
            #error "Not implemented"
        #endif
        std::shared_ptr<ul::Logger> mpLogger;

        bool mIsConnected = false;

        SemaphoreHandle_t mConnectionDataMutex = nullptr;

        TimerHandle_t mConnectionTimeoutTimer = nullptr;
    };
}