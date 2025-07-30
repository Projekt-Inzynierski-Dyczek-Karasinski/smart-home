#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

class Communication; 
// TODO add @pre, @post etc.

/**
 * @brief Driver class for HC12 module. Enables transmiting, receiving data via HC12 module and setting up HC12 module.
 * 
 * @warning This class must be initialized only once inside constructor of Communication class (if HC12 module is used).
 * 
 */
class HC12 {
public:
    /**
     * @brief Construct for a HC12 class. Creates main and transmit tasks, 
     * queue for transmitting data and mutex. Starts HardwareSerial communication with HC12 module. 
     * Sets SET_PIN to OUTPUT and its state to HIGH. 
     * 
     * @param communication pointer to instance of Communication class.
     */
    explicit HC12(Communication *communication);
    /**
     * @brief Destructor of HC12 class. Deletes all freeRTOS objects and HardwareSerial. Sets SET_PIN to LOW.
     * 
     * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
     */
    ~HC12();

    /**
     * @brief Method that allows transmiting messages via HC12 module.
     * 
     * @param message message to transmit.
     * 
     */
    void addMessageToTransmit(const uint8_t *message);

    /**
     * @brief Method that allows setting up HC12 module.  
     * Creates queue for setup HC12 task and adds to it passed commands. Creates setup HC12 task.
     * 
     * @param commands commands to setup HC12 module. Must start with "HC" string.
     * It is also possible to pass multiple commands at once by separating them with '|' character (up to 5).
     * 
     * Commands:
     * 
     * - "HC"    - check if HC12 is responding 
     * 
     * - "HC+RB" - check setted baudrate
     * 
     * - "HC+RC" - check setted rf channel
     * 
     * - "HC+RF" - check setted HC12 mode
     * 
     * - "HC+RP" - check setted transmission power  
     * 
     * - "HC+RX" - check 4 above
     * 
     * - "HC+Bx" - set baudrate (change x to baudrate to set)
     * 
     * - "HC+Cx" - set rf channel (change x to rf channel to set)
     * 
     * - "HC+FUx"- set HC12 mode (change x to mode to set)
     * 
     * - "HC+Px" - set transmission power (change x to power to set)
     * 
     * - "HC+DEFAULT" - reset HC12 module to default settings
     * 
     * - "HC+SLEEP" - put HC12 module in sleep mode
     * 
     */
    void setupHC12(const uint8_t *commands); 

private:
    /**
     * @brief Create a freeRTOS Queue for transmitting messages.
     * 
     */
    void createQueue();
    /**
     * @brief Delete a freeRTOS Queue for transmitting messages.
     * 
     */
    void deleteQueue();
    /**
     * @brief Create a freeRTOS Queues for setup HC12.
     * 
     */
    void createSetupHC12Queues();
    /**
     * @brief Delete a freeRTOS Queues for setup HC12.
     * 
     */
    void deleteSetupHC12Queues();
    
    /**
     * @brief Main HC12 freeRTOS task. Reads all output from HC12 module and passes it to Communication class.
     * This task is responsible for suspending/deleting resuming/creating  other HC12 tasks.
     * 
     * @param parameters FreeRTOS task parameters.
     * 
     * @note This task runs continuously in the background.
     * 
     */
    static void HC12MainTask(void *parameters);
    /**
     * @brief Create Main HC12 task.
     * 
     */
    void createHC12MainTask();
    /**
     * @brief Delete main HC12 task.
     * 
     */
    void deleteHC12MainTask();

    /**
     * @brief FreeRTOS task that is responsible for transmitting messages. 
     * Transmits messages from a queue and ensures that is delay between sending messages.
     * 
     * @param parameters FreeRTOS task parameters.
     * 
     * @note This task is suspended after of no transmitting for 1 second and 
     * is resumed by when a new message appear for transmission.
     *
     */
    static void transmitTask(void *parameters);
    /**
     * @brief Create a Transmit task.
     * 
     */
    void createTransmitTask();
    /**
     * @brief Delete a Transmit task.
     * 
     */
    void deleteTransmitTask();
    
    /**
     * @brief FreeRTOS task that is responsible for setting up HC12 module.
     * 
     * @param parameters FreeRTOS task parameters.
     * 
     * @note This task is created only if setup is needed by Main HC12 task and is deleted when setup is finished.
     * 
     */
    static void setupHC12Task(void *parameters);
    /**
     * @brief Create a Setup HC12 Task and sets SET_PIN to LOW.
     * 
     */
    void createSetupHC12Task();
    /**
     * @brief Delete a Setup HC12 Task and sets SET_PIN to HIGH.
     * 
     */
    void deleteSetupHC12Task();

    static HC12 *mspHC12; // pointer to instance of this class
    Communication *mpCommunication; // pointer to instance of Communication class
    HardwareSerial *mpSerial; // pointer to HardwareSerial
    unsigned long mBaudRate; // TODO remove?

    // Notifications for Main HC12 task
    typedef enum : uint32_t {
        defaultStatusNotif = 0,
        waitingForSendConfirmationNotif,
        cancelWaitingForSendConfirmationNotif,
        suspendTransmitTaskNotif,

        // setup HC12 notifications
        createSetupHC12TaskNotif,
        deleteSetupHC12TaskNotif,
    } mHC12MainNotifications;

    // Mutex
    SemaphoreHandle_t mSendingDataMutex = NULL;
    
    // Queues 
    QueueHandle_t mTransmitQueue = NULL;
    QueueHandle_t mSetupHC12CommandsQueue = NULL;
    QueueHandle_t mSetupHC12ReceiveQueue = NULL;
    
    // Tasks
    TaskHandle_t mHC12MainTaskHandle = NULL;
    TaskHandle_t mTransmitTaskHandle = NULL;
    TaskHandle_t mSetupHC12TaskHandle = NULL;
};