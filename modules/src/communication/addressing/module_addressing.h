#pragma once

#include <Arduino.h>
#include <memory>
#include <nlohmann/json.hpp>

#include "communication/addressing/addressing.h"

namespace ul = Utils::Logging;

namespace Comms {
    class Communication;

    /**
     * @brief Handles the addressing procedure for a module.
     * @details Manages the module's MAC/IP addressing, radio frequency channel (if supported by the RF module), and coordination with FreeRTOS tasks, queues,
     * and timer for module the addressing procedure. Inherits addressing utilities from the Addressing base class.
     */
    class ModuleAddressing final : public Addressing {
    public:
        /**
         * @brief Constructs a ModuleAddressing object and sets default values for the module's addressing variables.
         * @param communication Pointer to the Communication object.
         * @param logger Shared pointer to the Logger instance.
         */
        ModuleAddressing(Communication *communication, const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Gets default central unit's rf channel.
         * @return RF Channel.
         */
        uint8_t getConnectionRFChannel() override;

        /**
         * @brief Returns the current RF channel used by the module.
         * @return The current RF channel number (if macro <code>RF_CHANNELS</code> is not defined return 0).
         * @note Thread-safe.
         */
        uint8_t getDefaultRFChannel() override;

        /**
         * @brief Gets the currently assigned IP address for the module.
         * @return IP address.
         * @note Thread-safe.
         */
        uint8_t getIPAddress() override;

        /**
         * @brief Checks if the given MAC address is valid.
         * @details If the module is not addressed yet (IP address is <code>NULL_IP</code>), it will accept any MAC address,
         * otherwise MAC address is valid if it is the same as <code>mProtocolMACAddress</code>.
         * @param mac MAC address to check.
         * @return True if MAC address is valid, false otherwise.
         * @note Thread-safe.
         */
        bool isMACValid(const uint8_t *mac) override;

        /**
         * @brief Checks if the given IP address is valid.
         * @details IP address is valid if it is the same as saved IP address
         * or in case when there is no IP address assigned (IP address is <code>NULL_IP</code>)
         * if IP address is <code>CENTRAL_UNIT_IP</code>.
         * @param ip IP address to check.
         * @return True if IP address is valid, false otherwise.
         * @note Thread-safe.
         */
        bool isIpValid(uint8_t ip) override;

    private:
        /**
         * @brief Static FreeRTOS task function. Handles message exchanges and addressing logic for the module.
         * @param parameters Task parameters (unused).
         */
        static void addressingTask(void* parameters);
        /**
         * @brief Creates the addressing task responsible for the module's addressing procedure.
         * Ensures only one instance of the task is running at a time.
         */
        void createAddressingTask() override;

        /**
         * @brief Static callback for handling the timeout timer for the addressing algorithm. Calls abortAddressingWithAbortMessage() method as callback.
         * @param xTimer The triggered FreeRTOS timer handle.
         */
        static void addressingTimersCallbacks(TimerHandle_t xTimer);
        /**
         * @brief Creates the FreeRTOS timer controlling the absolute duration of addressing process.
         */
        void createAddressingTimer() override;

        /**
         * @brief Updates the protocol MAC and IP address information for module and the notifies Communication class to restart tasks which uses this variables.
         * @param newMAC New MAC address array.
         * @param newIP New IP address to assign.
         * @note Thread-safe.
         */
        void updateAddressingData(const uint8_t *newMAC, uint8_t newIP);
        #ifdef RF_CHANNELS
            /**
             * @brief Overloaded: Updates protocol MAC, IP, and RF channel for this module and the notifies Communication class to restart tasks which uses this variables.
             * @param newMAC New MAC address array.
             * @param newIP New IP address to assign.
             * @param newRfChannel New RF channel to assign.
             * @note Thread-safe.
             */
            void updateAddressingData(const uint8_t *newMAC, uint8_t newIP, uint8_t newRfChannel);
        #endif

        /**
         * @brief Clears all data related to a new connection attempt.
         * Sends a command to the HC12 module (if used) to reset it to default settings.
         */
        void clearNewConnectionData() override;
        /**
         * @brief Completely aborts the addressing process and calls the clearNewConnectionData() method.
         */
        void abortAddressing() override;

        /**
         * @brief Saves addressing data to flash memory.
         */
        void saveAddressingData();

        /**
         * @brief Loads modules addressing data from flash memory.
         * @details If no data exists, the method returns without modifying the current state.
         * @note Thread-safe.
         */
        void loadAddressingData();

        static ModuleAddressing *mspAddressing; ///< Static pointer to a ModuleAddressing instance.

        /**
         * @brief Data structure for serializing module addressing information.
         */
        struct AddressingData {
            /**
             * @brief Constructor that deserializes addressing data from JSON.
             * @param json JSON object containing addressing data.
             */
            explicit AddressingData(const nlohmann::json& json);

            /**
             * @brief Constructor that initializes addressing data with provided values.
             * @param ip IP address.
             * @param mac Pointer to MAC address array.
             * @param rfc RF channel to store, default is 0 for null.
             */
            AddressingData(uint8_t ip, const uint8_t* mac, uint8_t rfc = 0);

            /**
             * @brief Serializes the addressing data to JSON format.
             * @return JSON object containing all addressing data fields.
             */
            nlohmann::json toJson();

            uint8_t ipAddress = NULL_IP;
            uint8_t rfChannel = 0; // 0 for null
            uint8_t macAddress[MAC_ADDRESS_LENGTH] = {0,0,0,0,0,0};

        private:
            //json keys
            static constexpr char ms_JK_IP[] = "ip";
            static constexpr char ms_JK_RF_CHANNEL[] = "rfChannel";
            static constexpr char ms_JK_MAC_ADDRESS[] = "mac";
        };

        #ifdef RF_CHANNELS
            // TODO before merge with main remove commented code/rollback atomic
            // uint8_t mRfChannel = DEFAULT_CHANNEL; ///< Module's current RF channel.
            std::atomic<uint8_t> mRfChannel{DEFAULT_CHANNEL}; ///< Module's current RF channel.
        #endif
    };
}