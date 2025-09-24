#pragma once

#include <Arduino.h>
#include <memory>

#include "config/communication_config.h"
#include "config/addressing_config.h"
#include "utils/logger.h"


namespace ul = Utils::Logging;

namespace Comms {
    class Communication;

    /**
     * @brief Abstract base class for ModuleAddressing and CentralUnitAddressing classes.
     *        Responsible for handling MAC and IP addresses.
     */
    class Addressing {
    public:
        /**
         * @brief Constructs an Addressing object.
         * @param communication Pointer to the Communication object.
         * @param logger Shared pointer to the Logger instance.
         */
        Addressing(Communication *communication, const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Deletes FreeRTOS resources used by the class.
         * @note Virtual destructor for safe polymorphic deletion.
         */
        virtual ~Addressing();

        /**
         * @brief Copies the current protocol MAC address into the provided buffer.
         * @param macAddress Output buffer of length 6 for the MAC address.
         * @note Thread-safe.
         */
        void getProtocolMACAddress(uint8_t macAddress[MAC_ADDRESS_LENGTH]) const;

        // TODO !BEFORE PULL REQUEST! check comment
        /**
         * @brief Pure virtual getter for rf channel when device starts new connection.
         * @return RF Channel.
         * @warning Must be implemented by derived class.
         */
        virtual uint8_t getConnectionRFChannel() = 0;

        // TODO !BEFORE PULL REQUEST! check comment
        /**
         * @brief Pure virtual getter for rf channel when device is listening.
         * @return RF Channel.
         * @warning Must be implemented by derived class.
         */
        virtual uint8_t getDefaultRFChannel() = 0;

        /**
         * @brief Pure virtual getter for IP address.
         * @return IP address.
         * @warning Must be implemented by derived class.
         */
        virtual uint8_t getIPAddress() = 0;

        /**
         * @brief Virtual setter for IP address.
         * @return IP address.
         * @warning Must be implemented by CentralUnitAddressing class.
         * @note By default, this method does nothing.
         */
        virtual void setProtocolIPAddress(uint8_t ip);

        /**
         * @brief Pure virtual function checking if given mac address is propper.
         * @param mac MAC address to check.
         * @return True if MAC address is propper, false otherwise.
         * @warning Must be implemented by derived class.
         */
        virtual bool isMACPropper(const uint8_t* mac) = 0;

        /**
         * @brief Pure virtual function checking if given IP address is propper.
         * @param ip IP address to check.
         * @return True if IP address is propper, false otherwise.
         * @warning Must be implemented by derived class.
         */
        virtual bool isIpPropper(uint8_t ip) = 0;

        /**
         * @brief Initializes addressing procedures and related FreeRTOS resources.
         */
        void startAddressing();

        /**
         * @brief Stops addressing procedures and cleans up FreeRTOS resources.
         */
        void stopAddressing();

        /**
         * @brief Getter for <code>mIsAddressingWorking</code>.
         * @return True if addressing algorithm is in progress, false otherwise.
         * @note Thread-safe.
         */
        bool getIsAddressingWorking() const;

        /**
         * @brief Adds a message to the addressing queue. Ensures that adding messages to queue is only available if queue exist.
         * @param message Array with message array to add to queue.
         */
        void addMessage(const uint8_t message[MESSAGE_SIZE]) const;

    protected:
        /**
         * @brief Creates the FreeRTOS queue used for addressing messages.
         */
        void createAddressingQueue();

        /**
         * @brief Deletes the addressing queue and frees related resources.
         */
        void deleteAddressingQueue();

        /**
         * @brief Pure virtual function for creating the addressing task.
         * @warning Must be implemented by derived class.
         */
        virtual void createAddressingTask() = 0;

        /**
         * @brief Deletes the addressing task and frees related resources.
         */
        void deleteAddressingTask();

        /**
         * @brief Pure virtual function for creating the addressing timer.
         * @warning Must be implemented by derived class.
         */
        virtual void createAddressingTimer() = 0;

        /**
         * @brief Deletes the addressing timer and frees resources.
         */
        void deleteAddressingTimer();

        /**
         * @brief Pure virtual function to clear all data of a new connection.
         * @warning Must be implemented by derived class.
         */
        virtual void clearNewConnectionData() = 0;

        /**
         * @brief Sends a message to restart the addressing algorithm and calls clearNewConnectionData() method.
         */
        void sendRestartMessage();

        /**
         * @brief Pure virtual function to completely abort the addressing process.
         * @warning Must be implemented by derived class.
         */
        virtual void abortAddressing() = 0;

        /**
         * @brief Sends a message to completely abort the addressing process and calls abortAddressing() method.
         */
        void abortAddressingWithAbortMessage();

        /**
         * @brief Checks if addressing should be continued after receiving message.
         * @details If received message is to abort addressing, then will handle abort.
         * If received message is to restart addressing, then it will return true.
         * Otherwise, it will return false.
         * @param receiveBuffer Received message.
         * @return False if addressing should be continued, true otherwise.
         * @warning This method must not be called outside addressing task.
         * @note If received message is to abort addressing, technically function returns true,
         * but infinite loop prevents getting to "return" line (because task calling function will be deleted).
         */
        bool isAddressingFailed(const uint8_t *receiveBuffer);

        // TODO move this method to HC12 class when adding handling with propper mac and ip in decoding message task
        #ifdef HC12_MODULE
            /**
             * @brief Prepares and sends HC12 command to change RF channel.
             * @param newRfChannel New RF channel.
             */
            void changeRfChannel(uint8_t newRfChannel) const;
        #endif
        // TODO add led blinking indicating restart, success and failure of addressing
        // void restartLedBlink();
        // void successLedBlink();
        // void abortLedBlink();

        Communication *mpCommunication; ///< Pointer to the Communication class instance (owner class).

        bool mIsAddressingWorking = false; ///< Indicates if addressing algorithm is in progress.
        uint8_t mMACAddress[MAC_ADDRESS_LENGTH]{}; ///< Module's own MAC address.
        uint8_t mProtocolMACAddress[MAC_ADDRESS_LENGTH]{}; ///< Central unit's MAC address (or module's own MAC if unknown).
        uint8_t mIPAddress = NULL_IP; ///< Current assigned IP address (0 = NULL, 1 = central unit's IP).

        #ifdef ESP32_BOARD
            const bool m_IS_MAC_ADDRESS_REAL = true; ///< Indicates if MAC address is hardware-based (always true for ESP32).
        #else
            #error "MAC address not implemented!"
        #endif

        SemaphoreHandle_t mAddressingDataMutex = nullptr; ///< Handle to mutex protecting access to addressing data.

        QueueHandle_t mAddressingQueue = nullptr; ///< Handle to FreeRTOS queue for incoming addressing messages, queue length: 10x64 bytes (uint8_t).

        TaskHandle_t mAddressingTaskHandle = nullptr; ///< Handle to the addressing FreeRTOS task.

        TimerHandle_t mAddressingTimeoutTimer = nullptr; ///< Handle to the addressing timeout FreeRTOS software timer.

        std::shared_ptr<ul::Logger> mpLogger;
    };
}