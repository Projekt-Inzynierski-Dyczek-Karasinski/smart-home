#pragma once

#include <HardwareSerial.h>
#include <memory>

#include "utils/logger.h"

namespace ul = Utils::Logging;
class Communication; 
// TODO add @pre, @post etc.

/**
 * @brief Driver class for HC12 module. Enables transmitting, receiving data via HC12 module and setting up HC12 module.
 * @warning This class must be initialized only once inside constructor of Communication class (if HC12 module is used).
 */
class HC12 {
public:
    /**
     * @brief Constructor for a HC12 class. Creates main and transmit tasks, 
     * queue for transmitting data and mutex. Starts HardwareSerial communication with HC12 module. 
     * Sets SET_PIN to OUTPUT and its state to HIGH. 
     * @param communication pointer to instance of Communication class.
     * @param logger Shared pointer to the Logger instance.
     */
    HC12(Communication *communication, const std::shared_ptr<ul::Logger> &logger);
    /**
     * @brief Destructor of HC12 class. Deletes all FreeRTOS objects and HardwareSerial. Sets SET_PIN to LOW.
     * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
     */
    ~HC12();

    /**
     * @brief Method that allows transmitting messages via HC12 module.
     * @param message message to transmit.
     */
    void addMessageToTransmit(const uint8_t *message) const;

    /**
     * @brief Method that allows setting up HC12 module.  
     * Creates a queue for the setup HC12 task and adds the passed commands to it. Creates setup HC12 task.
     * 
     * @param commands commands to setup HC12 module. Must start with "HC" string.
     * It is also possible to pass multiple commands at once by separating them with '|' character (up to 5).
     * Commands:
     * - "HC"    - check if HC12 is responding 
     * - "HC+RB" - check te baudrate
     * - "HC+RC" - check set rf channel
     * - "HC+RF" - check set HC12 mode
     * - "HC+RP" - check set transmission power  
     * - "HC+RX" - check 4 above
     * - "HC+Bx" - set baudrate (change x to baudrate to set)
     * - "HC+Cx" - set rf channel (change x to rf channel to set)
     * - "HC+FUx"- set HC12 mode (change x to mode to set)
     * - "HC+Px" - set transmission power (change x to power to set)
     * - "HC+DEFAULT" - reset HC12 module to default settings
     * - "HC+SLEEP" - put HC12 module in sleep mode
     */
    void setupHC12(const uint8_t *commands); 

private:
    /**
     * @brief Create a FreeRTOS Queues used in HC12 class.
     */
    void createQueues();
    /**
     * @brief Delete a FreeRTOS Queues used in HC12 class.
     */
    void deleteQueues();
    /**
     * @brief Create a FreeRTOS Queues for setup HC12.
     */
    void createSetupHC12Queues();
    /**
     * @brief Delete a FreeRTOS Queues for setup HC12.
     */
    void deleteSetupHC12Queues();
    
    
    /**
     * @brief Decides what to do with output from HC12 module.
     * @param hc12Output Pointer to variable storing output from HC12 module.
     * @param isSetupHC12Working Pointer to if the setup task is sending commands to HC12.
     * @param isWaitingForSendConfirmation Pointer to variable if is being waited for confirmation from HC12.
     */
    void hc12OutputDecider(const uint8_t *hc12Output, const bool *isSetupHC12Working, bool *isWaitingForSendConfirmation) const;
    /**
     * @brief Main HC12 FreeRTOS task. Reads all output from HC12 module and passes it to Communication class.
     * This task is responsible for suspending/deleting resuming/creating other HC12 tasks.
     * @param parameters FreeRTOS task parameters.
     * @note This task runs continuously in the background.
     */
    static void HC12MainTask(void *parameters);
    /**
     * @brief Create Main HC12 task.
     */
    void createHC12MainTask();
    /**
     * @brief Delete main HC12 task.
     */
    void deleteHC12MainTask();

    /**
     * @brief FreeRTOS task that is responsible for transmitting messages. 
     * Transmits messages from a queue and ensures that is delay between sending messages.
     * @param parameters FreeRTOS task parameters.
     * @note This task is suspended after no transmitting for 1 second and 
     * is resumed when a new message appear for transmission.
     */
    static void transmitTask(void *parameters);
    /**
     * @brief Create a Transmit task.
     */
    void createTransmitTask();
    /**
     * @brief Delete a Transmit task.
     */
    void deleteTransmitTask();
    
    /**
     * @brief FreeRTOS task that is responsible for setting up HC12 module.
     * @param parameters FreeRTOS task parameters.
     * @note This task is created only if setup is needed by Main HC12 task and is deleted when setup is finished.
     */
    static void setupHC12Task(void *parameters);
    /**
     * @brief Create a Setup HC12 Task and sets SET_PIN to LOW.
     */
    void createSetupHC12Task();
    /**
     * @brief Delete a Setup HC12 Task and sets SET_PIN to HIGH.
     */
    void deleteSetupHC12Task();

    static HC12 *mspHC12; // pointer to instance of this class
    Communication *mpCommunication; // pointer to instance of Communication class
    HardwareSerial *mpSerial; // pointer to HardwareSerial
    unsigned long mBaudRate; // TODO remove?

    // Notifications for Main HC12 task
    typedef enum : uint8_t {
        DEFAULT_STATUS_NOTIF = 0,
        WAITING_FOR_SEND_CONFIRMATION_NOTIF,
        CANCEL_WAITING_FOR_SEND_CONFIRMATION_NOTIF,
        SUSPEND_TRANSMIT_TASK_NOTIF,

        // setup HC12 notifications
        CREATE_SETUP_HC12_TASK_NOTIF,
        DELETE_SETUP_HC12_TASK_NOTIF,
    } mHC12MainNotifications;

    SemaphoreHandle_t mSendingDataMutex = nullptr; ///< Handle to FreeRTOS mutex protecting access to UART transmission to HC12 module.
    
    QueueHandle_t mMainNotificationsQueue = nullptr; ///< Handle to FreeRTOS queue for notifications for the Main task, queue length: 5 bytes (uint8_t).
    QueueHandle_t mTransmitQueue = nullptr; ///< Handle to FreeRTOS queue for (encoded) messages to transmit, queue length: 11x16 bytes (uint8_t).
    QueueHandle_t mSetupHC12CommandsQueue = nullptr; ///< Handle to FreeRTOS queue for HC12 commands, queue length: 5x10 bytes (uint8_t).
    QueueHandle_t mSetupHC12ReceiveQueue = nullptr; ///< Handle to FreeRTOS queue for response from HC12 after sending command, queue length: 43 bytes (uint8_t).
    
    TaskHandle_t mHC12MainTaskHandle = nullptr; ///< Handle to FreeRTOS main HC12 task.
    TaskHandle_t mTransmitTaskHandle = nullptr; ///< Handle to FreeRTOS transmission task.
    TaskHandle_t mSetupHC12TaskHandle = nullptr; ///< Handle to FreeRTOS setup task.

    std::shared_ptr<ul::Logger> mpLogger;
};