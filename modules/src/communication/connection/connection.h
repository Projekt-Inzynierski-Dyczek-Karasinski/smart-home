#pragma once

#include <memory>

#include "../config/communication_config.h"

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

    /**
     * @brief Thread-safe Meyers Singleton for managing RF communication connections.
     * Handles connection establishment and message retransmission logic
     * ("repeat" message) for RF communication between devices.
     */
    class Connection {
    public:
        /**
         * @brief Provides access to the singleton instance of the Communication class.
         * @param communication Pointer to Communication class instance.
         * @param addressing Shared pointer to CentralUnitAddressing/ModuleAddressing class.
         * @param rfModule Shared pointer to RF module.
         * @param logger Shared pointer to logger.
         * @return Reference to singleton Connection instance.
         */
        static Connection& getInstance(
            Communication *communication,
            #ifdef CENTRAL_UNIT
                const std::shared_ptr<CentralUnitAddressing> &addressing,
            #else
                const std::shared_ptr<ModuleAddressing> &addressing,
            #endif
            const std::shared_ptr<HC12> &rfModule,
            const std::shared_ptr<ul::Logger> &logger
        );

        // Delete copy constructor and assignment operator
        Connection(const Connection&) = delete;
        Connection& operator = (const Connection&) = delete;

        /**
         * @brief Process and handle incoming connection-related messages.
         * @param receivedMessage Array of <code>MESSAGE_SIZE</code> bytes containing the received message.
         */
        void messageDecider(const uint8_t receivedMessage[MESSAGE_SIZE]);

        /**
         * @brief Handle connection when receiving new RF message.
         * @details Establishes connection, creates and starts/restarts timeout timer, and sets protocol IP address.
         * @param ip IP address of the connecting device
         * @note If addressing is in process, does nothing.\n
         * Thread-safe.
         */
        void receivingHandle(uint8_t ip);

        /**
         * @brief Handle connection when sending new RF message.
         * @details Establishes connection, stores last transmitted message for potential retransmission,
         * and configures RF channel if needed.
         * @param message Array of <code>MESSAGE_SIZE</code> bytes to be transmitted.
         * @note If addressing is in process, only handles last transmitted message.\n
         * Thread-safe.
         */
        void sendingHandle(const uint8_t message[MESSAGE_SIZE]);

        /**
        * @brief Terminate current connection and cleanup resources.
        * @details Stops and deletes connection timeout timer, resets connection state variables,
        * and sets default RF channel. Sets protocol IP to NULL_IP for Central Unit.
        * @note Thread-safe.
        */
        void endConnection();

        /**
         * @brief Transmit a "repeat" message to request re-transmission.
         * Used when checksum or last byte of message is incorrect.
         * @note Thread-safe.
         */
        void transmitRepeatMessage() const;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         *        Initializes FreeRTOS mutex and prepares buffer for last transmitted message.
         * @param communication Pointer to Communication class instance.
         * @param addressing Shared pointer to addressing module.
         * @param rfModule Shared pointer to RF module.
         * @param logger Shared pointer to logger.
         */
        Connection(
            Communication *communication,
            #ifdef CENTRAL_UNIT
                const std::shared_ptr<CentralUnitAddressing> &addressing,
            #else
                const std::shared_ptr<ModuleAddressing> &addressing,
            #endif
            const std::shared_ptr<HC12> &rfModule,
            const std::shared_ptr<ul::Logger> &logger
        );

        /**
         * @brief Destructor cleans up FreeRTOS resources.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         */
        ~Connection();

        /**
         * @brief Static callback function for FreeRTOS timer.
         * @details Calls <code>endConnection()</code> method when connection timeout occurs.
         * @param xTimer Handle to the timer that triggered the callback.
         */
        static void connectionTimerCallback(TimerHandle_t xTimer);

        /**
         * @brief Create timer used in Connection class.
         * @note Only creates timer if timer is not already created.
         */
        void createConnectionTimer();
        /**
         * @brief Delete timer used in Connection class.
         * @note Only deletes timer if timer exists.
         */
        void deleteConnectionTimer();

        /**
         * @brief Set the last transmitted RF message value.
         * @param message Message to set.
         * @warning Do not set "repeat" message to last transmitted message, that may cause spamming "repeat" messages if RF transmission is disturbed.
         * @note Thread-safe.
         */
        void setLastTransmittedMessage(const uint8_t message[MESSAGE_SIZE]);
        /**
         * @brief Resend the last message, if the repeat count is not exceeded.
         * Used when get "repeat" message.
         * @note Thread-safe.
         */
        void repeatLastTransmittedMessage();

        // Pointers
        static Connection *mspConnection; ///< Static pointer to this class instance.
        Communication *mpCommunication; ///< Pointer to the Communication class instance.
        #ifdef CENTRAL_UNIT
            std::shared_ptr<CentralUnitAddressing> mpAddressing; ///< Pointer to CentralUnitAddressing class instance.
        #else
            std::shared_ptr<ModuleAddressing> mpAddressing; ///< Pointer to ModuleAddressing class instance.
        #endif
        #ifdef HC12_MODULE
            std::shared_ptr<HC12> mpRfModule; //< Pointer to class instance responsible for transmitting and receiving RF messages.
        #else
            #error "Not implemented"
        #endif
        std::shared_ptr<ul::Logger> mpLogger;

        // Class variables
        bool mIsConnected = false; //< Flag indicating current connection status.
        uint8_t mLastTransmittedMessage[MESSAGE_SIZE]{}; ///< Recently transmitted message (for "repeat" logic).
        uint8_t mLastTransmittedMessageAttempts = 0; ///< Counter of repeats of last sent message.

        // FreeRTOS
        SemaphoreHandle_t mConnectionDataMutex = nullptr; ///< FreeRTOS mutex protecting Connection class variables.
        TimerHandle_t mConnectionTimeoutTimer = nullptr; ///< FreeRTOS software timer for connection timeout management.
    };
}