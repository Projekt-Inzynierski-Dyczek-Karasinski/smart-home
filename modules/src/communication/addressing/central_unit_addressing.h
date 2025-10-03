#pragma once

#include <Arduino.h>
#include <memory>

#include "communication/addressing/addressing.h"

// assuming that central unit is using hc12 to rf communication
#ifndef HC12_MODULE
    #error "Central unit must run with hc12 module"
#endif

namespace ul = Utils::Logging;

namespace Comms {
    class Communication;

    /**
     * @brief Manages the addressing procedure for the central unit.
     * @details Handles adding and removing of modules, assignment and tracking of MAC/IP/RF channel
     * and coordination of FreeRTOS task, timer and queue required for addressing logic.
     * Ensures thread-safe updates and provides centralized management of addressing data.
     * Inherits addressing utilities from the Addressing base class.
    */
    class CentralUnitAddressing final : public Addressing {
    public:
        /**
         * @brief Constructs a CentralUnitAddressing object and sets default values for addressing variables.
         * @param communication Pointer to the Communication object.
         * @param logger Shared pointer to the Logger instance.
         */
        CentralUnitAddressing(Communication *communication, const std::shared_ptr<ul::Logger> &logger);
        /**
         * @brief Destructor. Cleans up FreeRTOS resources used by class.
         * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
         */
        ~CentralUnitAddressing() override;

        /**
         * @brief Gets rf channel of the module which central unit is trying to connect.
         * @return RF Channel.
         * @note Thread-safe.
         */
        uint8_t getConnectionRFChannel() override;

        /**
         * @brief Gets default rf channel.
         * @return RF channel.
         */
        uint8_t getDefaultRFChannel() override;

        /**
         * @brief Gets the IP address of module which is currently communicating with central unit (or cental unit's IP if is start of addressing).
         * @return IP address.
         * @note Thread-safe.
         */
        uint8_t getIPAddress() override;

        /**
         * @brief Checks if a module with the passed IP address is addressed and sets the protocol IP if it is addressed.
         * @param ip IP address to set, <code>NULL_IP</code> to clear.
         * @note Thread-safe.
         */
        void setProtocolIPAddress(uint8_t ip) override;

        /**
         * @brief Checks if given MAC address is valid.
         * @details If the central unit is waiting for a new connection message, it will accept any MAC address,
         * otherwise checks if the given MAC address is the same as <code>mProtocolMACAddress</code>.
         * @param mac MAC address to check.
         * @return True if MAC address is valid, false otherwise.
         * @note Thread-safe.
         */
        bool isMACValid(const uint8_t *mac) override;

        /**
         * @brief Checks if the given IP address is valid.
         * @details If the central unit is waiting for a new connection message, it will accept only IP address = <code>NULL_IP</code>,
         * otherwise it accepts any IP address that isn't <code>NULL_IP</code> or <code>CENTRAL_UNIT_IP</code>.
         * @param ip IP address to check.
         * @return True if IP address is valid, false otherwise.
         * @note Thread-safe.
         */
        bool isIpValid(uint8_t ip) override;

    private:
        /**
         * @brief Structure holding addressing data for a module.
         */
        struct AddressingData {
            bool isMACAddressReal = true; ///< True if MAC address is a real hardware address, false otherwise.
            uint8_t ipAddress = NULL_IP; ///< Module's IP address.
            uint8_t rfChannel = DEFAULT_CHANNEL; ///< Module's RF channel.
            uint8_t macAddress[6] = {0, 0, 0, 0, 0, 0}; ///< Module's MAC address.
        };

        /**
         * @brief Prepare buffer for sending summary data to the module.
         * @param sendBuffer Buffer to be prepared.
         * @param moduleData Data of the module which is addressing.
         */
        void prepareSummary(uint8_t *sendBuffer, const AddressingData *moduleData);
        /**
         * @brief Static FreeRTOS task function. Handles message exchanges and addressing logic of the central unit's addressing procedure.
         * @param parameters Task parameters (unused).
         */
        static void addressingTask(void* parameters);
        /**
         * @brief Creates the FreeRTOS task responsible for the central unit's addressing process.
         */
        void createAddressingTask() override;
        /**
         * @brief Static callback for handling the timeout timer for the addressing algorithm.
         * Calls abortAddressingWithAbortMessage() method as callback.
         * @param xTimer The triggered FreeRTOS timer handle.
         */
        static void addressingTimersCallbacks( TimerHandle_t xTimer);
        /**
         * @brief Creates the FreeRTOS timer controlling the absolute duration of addressing process.
         */
        void createAddressingTimer() override;

        /**
         * @brief Prints information about all modules' addressing data to the serial output.
         * @warning This is special debug method that prints by its own. That print is <b>not</b> thread-safe, (only printing is not thread-safe).
         */
        void printModulesAddressingData() const;
        /**
         * @brief Prints the number of modules registered on each RF channel to the serial output.
         * @warning This is special debug method that prints by its own. That print is <b>not</b> thread-safe, (only printing is not thread-safe).
         */
        void printNumOFModulesOnRfChannels() const;

        /**
         * @brief Retrieves the addressing data for the module with the given IP address.
         * @param addressingData Pointer to an AddressingData structure to be filled.
         * @param ipAddress IP address of the module to get data for.
         * @note Thread-safe.
         */
        void getModuleData(AddressingData *addressingData, uint8_t ipAddress) const;
        /**
         * @brief Gets the RF channel assigned to a module with the given IP address.
         * @param ipAddress IP address of the module.
         * @return Assigned RF channel for the specified module.
         * @note Thread-safe.
         */
        uint8_t getModuleRfChannel(uint8_t ipAddress) const;

        /**
         * @brief Adds a new module to the addressing table.
         * Assigns an IP address, RF channel, and stores MAC information.
         * @param macAddress Pointer to the MAC address array of the module.
         * @param isMACAddressReal True if MAC address is real, false if fake.
         * @param rfChannel RF channel to assign, or 0 for automatic selection.
         * @return Assigned IP address for the new module, or NULL_IP (0) if assignment failed.
         * @note Thread-safe.
         */
        uint8_t addModule(const uint8_t *macAddress, bool isMACAddressReal, uint8_t rfChannel = 0);
        /**
         * @brief Removes a module from the addressing table using its IP address.
         * Frees up its IP and RF channel slot for a new module.
         * @param ipAddress IP address of the module to remove.
         * @note Thread-safe.
         */
        void removeModule(uint8_t ipAddress);

        /**
         * @brief Gets the temporary IP address assigned to the module currently being addressed.
         * @return IP address of the current module in addressing, or NULL_IP (0) if none.
         * @note Thread-safe.
         */
        uint8_t getTmpModuleIp() const;

        /**
         * @brief Sets the temporary IP address assigned to the module currently being addressed.
         * @param ip IP to set.
         * @note Thread-safe.
         */
        void setTmpModuleIp(uint8_t ip);

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
         * @brief Getter for <code>mIsStartOfAddressing</code>.
         * @return Value of <code>mIsStartOfAddressing</code>.
         * @note Thread-safe.
         */
        bool getIsStartOfAddressing() const;

        /**
         * @brief Setter for <code>mIsStartOfAddressing</code>.
         * @param value Value to set <code>mIsStartOfAddressing</code>.
         * @note Thread-safe.
         */
        void setIsStartOfAddressing(bool value);

        static CentralUnitAddressing *mspAddressing; ///< Static pointer to a CentralUnitAddressing instance.

        AddressingData mModulesAddressingData[MAX_NUM_OF_MODULES]; ///< Array containing addressing data for all registered modules.
        uint8_t mNumOFModulesOnRfChannel[MAX_CHANNEL]{}; ///< Array containing the number of modules assigned to each RF channel.
        uint8_t mTmpModuleIp = NULL_IP; ///< IP address of the module currently being addressed.
        bool mIsStartOfAddressing = false; ///< Flag indicating that it is waiting for a new connection message, so will accept any MAC address.

        SemaphoreHandle_t mModulesAddressingDataMutex = nullptr; ///< Handle to mutex protecting access to modules addressing data.
    };
}