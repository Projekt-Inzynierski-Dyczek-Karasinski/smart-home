#pragma once

#include <Arduino.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "communication/addressing/addressing.h"

class Communication; 

// assuming that central unit is using hc12 to rf communication
#ifndef HC12_MODULE
    #error "Central unit must run with hc12 module"
#endif

// TODO add @details
/**
 * @brief Manages the addressing procedure for the central unit.
 *  
 * Handles adding and removing of modules, assignment and tracking of MAC/IP/RF channel 
 * and coordination of FreeRTOS task, timer and queue required for addressing logic.
 * Ensures thread-safe updates and provides centralized management of addressing data. 
 * Inherits addressing utilities from the Addressing base class.
*/
class CentralUnitAddressing final : public Addressing{
public:
    /**
     * @brief Constructs a CentralUnitAddressing object and sets default values for addressing variables.
     * @param communication Pointer to the Communication object.
     */
    explicit CentralUnitAddressing(Communication *communication);
    /**
     * @brief Destructor. Cleans up FreeRTOS resources (task, queue, timer) used by class.
     * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
     */
    ~CentralUnitAddressing();

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
    static void addressingTimersCallbacks(TimerHandle_t xTimer);
    /**
     * @brief Creates the FreeRTOS timer controlling the absolute duration of addressing process.
     */
    void createAddressingTimer() override;
    
    /**
     * @brief Prints information about all modules' addressing data to the serial output.
     * Ensures thread-safe access.
     */
    void printModulesAddressingData();
    /**
     * @brief Prints the number of modules registered on each RF channel to the serial output.
     * Ensures thread-safe access.
     */
    void printNumOFModulesOnRfChannels();

    /**
     * @brief Retrieves the addressing data for the module with the given IP address.
     * Ensures thread-safe access.
     * @param addressingData Pointer to an AddressingData structure to be filled.
     * @param ipAddress IP address of the module to get data for.
     */
    void getModuleData(AddressingData *addressingData, const uint8_t ipAddress);
    /**
     * @brief Gets the RF channel assigned to a module with the given IP address.
     * Ensures thread-safe access.
     * @param ipAddress IP address of the module.
     * @return Assigned RF channel for the specified module.
     */
    uint8_t getModuleRfChannel(const uint8_t ipAddress);

    /**
     * @brief Adds a new module to the addressing table.
     * Assigns an IP address, RF channel, and stores MAC information.
     * Ensures thread-safe access.
     * @param macAddress Pointer to the MAC address array of the module.
     * @param isMACAddressReal True if MAC address is real, false if fake.
     * @param rfChannel RF channel to assign, or 0 for automatic selection.
     * @return Assigned IP address for the new module, or NULL_IP (0) if assignment failed.
     */
    uint8_t addModule(const uint8_t *macAddress, const bool isMACAddressReal, uint8_t rfChannel = 0);
    /**
     * @brief Removes a module from the addressing table using its IP address.
     * Frees up its IP and RF channel slot for a new module.
     * Ensures thread-safe access.
     * @param ipAddress IP address of the module to remove.
     */
    void removeModule(const uint8_t ipAddress);

    /**
     * @brief Gets the temporary IP address assigned to the module currently being addressed.
     * Ensures thread-safe access.
     * @return IP address of the current module in addressing, or NULL_IP (0) if none.
     */
    uint8_t getTmpModuleIp();

    /**
     * @brief Clears all data related to a new connection attempt.
     * Sends a command to the HC12 module (if used) to reset it to default settings.
     */
    void clearNewConnectionData() override;

    /**
     * @brief Completely aborts the addressing process and calls the clearNewConnectionData() method.
     */
    void abortAddressing() override;
    
    static CentralUnitAddressing *mspAddressing; ///< Static pointer to a CentralUnitAddressing instance.

    AddressingData mModulesAddressingData[MAX_NUM_OF_MODULES]; ///< Array containing addressing data for all registered modules.
    uint8_t mNumOFModulesOnRfChannel[MAX_NUM_OF_CHANNEL]; ///< Array containing the number of modules assigned to each RF channel.
    uint8_t mTmpModuleIp = NULL_IP; ///< IP address of the module currently being addressed. // TODO remember to clear that after end of new connection

    SemaphoreHandle_t mModulesAddressingDataMutex = NULL; ///< Handle to mutex protecting access to modules addressing data.
};