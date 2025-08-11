#pragma once

#include <Arduino.h>
#include <memory>

#include "smart_home_config.h"
#include "config/addressing_config.h"

#include "communication/addressing/addressing.h"

class Communication;
namespace ul = Utils::Logging;
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
     * @brief Destructor. Cleans up FreeRTOS resources (task, queue, timer) used by class.
     * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
     */
    ~ModuleAddressing() override;

    #ifdef RF_CHANNELS
        /**
         * @brief Returns the current RF channel used by the module.
         * @return The current RF channel number.
         * @note Thread-safe.
         */
        uint8_t getRfChannel() const;
    #endif

private:
    /**
     * @brief Static FreeRTOS task function. Handles message exchanges and addressing logic for the module.
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
 
    static ModuleAddressing *mspAddressing; ///< Static pointer to a ModuleAddressing instance.

    #ifdef RF_CHANNELS
        uint8_t mRfChannel = DEFAULT_CHANNEL; ///< Module's current RF channel.
    #endif
};