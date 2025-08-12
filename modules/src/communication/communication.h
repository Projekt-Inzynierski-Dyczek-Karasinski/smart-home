#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <memory>

#include "../smart_home_config.h"
#include "config/communication_config.h"

#ifdef HC12_MODULE
    #include "communication/hc12.h"
#endif
#ifdef CENTRAL_UNIT 
    #include "communication/addressing/central_unit_addressing.h"
#else
    #include "communication/addressing/module_addressing.h"
#endif

#include "universal_module_system/debug_led.h"
#include "utils/logger.h"

namespace ul = Utils::Logging;

/**
 * @class Communication
 * @brief Thread-safe Meyers Singleton class that manages all aspects of inter-module communication, protocol encoding/decoding, 
 * (using the HC12 class) transmission/reception of RF messages 
 * and (using the ModuleAddressing or CentralUnitAddressing classes) addressing. 
 * Additionally manages serial (terminal) input for testing.
 */
class Communication {
public:
    /**
     * @brief Provides access to the singleton instance of the Communication class.
     * @param debugLED Pointer to DebugLED instance.
     * @param logger Shared pointer to the Logger instance.
     * @return Reference to the singleton Communication instance.
     */
    static Communication& getInstance(DebugLED *debugLED, const std::shared_ptr<ul::Logger> &logger);
    
    // Delete copy constructor and assignment operator
    Communication(const Communication&) = delete;
    Communication& operator = (const Communication&) = delete;

    /**
     * @brief Start the addressing process. Initiates the necessary task and LED signaling for pairing.
     */
    void startAddressingAlgorithm() const;

    /**
     * @brief Signals the main task to stop the addressing process.
     */
    void stopAddressingAlgorithm() const;

    /**
     * @brief Signals the main task to not decode the next RF message (used at the start of the addressing process).
     */
    void needRawMessage() const;

    // TODO consider change so that don't be necessary
    /**
     * @brief Signals the main task to delete and recreate encode task (used after getting new IP address).
     */
    void resetEncodeMessageTask();

    /**
     * @brief Signals the main task to start pinging process.
     */
    void startPinging() const;

    /**
     * @brief Add a single byte to the byte queue for decoding.
     *        Resumes the decode task if necessary.
     * @param data Byte to add to receive queue.
     */
    void addByteToDecode(uint8_t data) const;

     /**
     * @brief Add a message that needs to be transmitted to the encoding queue. 
     * @param message Message to transmit.
     */
    void sendMessage(const uint8_t message[MESSAGE_SIZE]) const;
    
    /**
     * @brief Add a message to received queue (omitting decoding process).
     *        Used for internal communication of different parts of module.
     * @param message Message.
     */
    void sendInternalMessage(const uint8_t message[MESSAGE_SIZE]) const;

private:
    /**
     * @brief Private constructor for singleton pattern.
     *        Initializes FreeRTOS queues, tasks, timers, semaphores necessary for all communication (internal and RF) works.
     * @param debugLED Pointer to DebugLED instance.
     * @param logger Shared pointer to the Logger instance.
     */
    explicit Communication(DebugLED *debugLED, const std::shared_ptr<ul::Logger> &logger);
    /**
     * @brief Destructor. Cleans up FreeRTOS resources used by the class.
     * @warning Destructor of this class exists only for programming principles. This class should never be deleted.
     */
    ~Communication();

    /**
     * @brief Create all required queues for Communication class.
     */
    void createCommunicationQueues();
    /**
     * @brief Delete all queues for Communication class.
     */
    void deleteCommunicationQueues();

    /**
     * @brief Decides what to do with incoming messages (both internal and RF).
     * @param isReadingRawMessage Pointer to flag for raw message state.
     */
    void receivedMessageDecider(bool *isReadingRawMessage);
    /**
     * @brief Handles when main task is in default state.
     * @param isReadingRawMessage Pointer to flag for raw message state.
     */
    void normalOperationHandling(bool *isReadingRawMessage);
    /**
     * @brief Handles when main task get notification about ping timeout.
     * @param pingAttempts Pointer to counter with number of ping attempts.
     */
    void pingTimeoutNotifHandling(uint8_t *pingAttempts) const;
    /**
     * @brief Main FreeRTOS task for Communication class. 
     * It is responsible for suspending/deleting resuming/creating other communication related tasks.
     * Coordinates work of different parts of system. 
     * Decide (using receivedMessageDecider() method) what to do with incoming messages.
     * Handles notifications from other tasks/timers.
     * @param parameters Task parameters (unused).
     * @note This task runs continuously in the background. 
     */ 
    static void communicationMainTask(void *parameters);
    /**
     * @brief Create the main communication FreeRTOS task.
     */
    void createCommunicationMainTask();
    /**
     * @brief Delete the main communication FreeRTOS task.
     */
    void deleteCommunicationMainTask();

    /**
     * @brief Verifies the checksum of a given protocol message.
     * @param message The protocol message array to verify checksum.
     * @return True if the checksum is correct, false otherwise.
     */
    bool isCheckSumCorrect(const uint8_t message[PROTOCOL_SIZE]);

    /**
     * @brief Extracts a message from protocolBuffer and saves it to given buffer. Checks for packet loss.
     * @param protocolBuffer Buffer with raw message.
     * @param messageBuffer Buffer to save extracted message.
     * @return True if successfully extracts a message (without packet loss), false otherwise.
     */
    bool extractMessageFromProtocolBuffer(const uint8_t protocolBuffer[][PROTOCOL_SIZE], uint8_t *messageBuffer);

    /**
     * @brief FreeRTOS task for decoding incoming messages from the byte queue.
     * @param parameters Task parameters (unused).
     * @note This task is suspended after of no getting bytes from the queue for 10 second and 
     * is resumed by when a new byte appear.
     */
    static void decodeMessageTask(void *parameters);
    /**
     * @brief Create the decode FreeRTOS task.
     */
    void createDecodeMessageTask();
    /**
     * @brief Delete the decode FreeRTOS task.
     */
    void deleteDecodeMessageTask();

    /**
     * @brief Calculates and sets checksum to given buffer.
     * @param protocolBuffer Buffer for which to calculate and set checksum.
     */
    void prepareChecksum(uint8_t protocolBuffer[PROTOCOL_SIZE]);
    /**
     * @brief FreeRTOS task responsible for encoding outgoing messages and adding them to the transmit queue.
     *        Formats messages according to protocol before transmission.
     * @param parameters Task parameters (unused).
     * @note This task is suspended after of no getting any message from the queue for 10 second and 
     * is resumed by when a new message appear.
     */
    static void encodeMessageTask(void *parameters);
    /**
     * @brief Create the encode FreeRTOS task.
     */
    void createEncodeMessageTask();
    /**
     * @brief Delete the encode FreeRTOS task.
     */
    void deleteEncodeMessageTask();

    /**
     * @brief FreeRTOS task for handling custom messages sent via Serial (for manual testing).
     *        Passes received commands/messages into protocol or internal processing.
     * @param parameters Pointer to parameters given by FreeRTOS (usually not used).
     * @note This task runs continuously in the background, but with big delay in main loop to minimize impact to other background tasks. 
     */
    static void sendCustomMessageTask(void *parameters);
    /**
     * @brief Create the custom message FreeRTOS task.
     */
    void createSendCustomMessageTask();
    /**
     * @brief Delete the custom message FreeRTOS task.
     */
    void deleteSendCustomMessageTask();

    /**
     * @brief FreeRTOS software timers callback for all communication timers.
     * @param xTimer Handle to the expired FreeRTOS timer.
     */
    static void communicationTimersCallbacks(TimerHandle_t xTimer);
    /**
     * @brief Create all required communication timers.
     */
    void createCommunicationTimers();
    /**
     * @brief Delete all required communication timers.
     */
    void deleteCommunicationTimers();

    /**
     * @brief Clears (sets all bytes to 0) the stored last transmitted RF message.
     * Clears the buffer and sets repeat attempts to zero.
     * @note Thread-safe.
     */
    void setLastTransmittedMessage();
    /**
     * @brief Set the last transmitted RF message value.
     * @param message Message to set.
     * @warning Do not set "repeat" message to last transmitted message, that may cause spamming "repeat" messages if RF transmission is disturbed.
     * @note Thread-safe.
     */
    void setLastTransmittedMessage(const uint8_t message[MESSAGE_SIZE]);
    /**
     * @brief Transmit a "repeat" message to request re-transmission.
     * Used when checksum or last byte of message is incorrect.
     */
    void transmitRepeatMessage() const;
    /**
     * @brief Resend the last message, if the repeat count is not exceeded.
     * Used when get "repeat" message.
     * @note Thread-safe.
     */
    void repeatLastTransmittedMessage();

    /**
     * @brief Transmit a "ping" message.
     */
    void transmitPing() const;
    /**
     * @brief Transmit a "reping" message, as a reply to a received "ping" message.
     */
    void replyToPing() const;

    // TODO change this to nonstatic if possible
    static Communication *mspCommunication;
    static DebugLED *mspDebugLED; ///< Pointer to debugLED class instance.

    #ifdef HC12_MODULE
        std::unique_ptr<HC12> mpRfModule; ///< Pointer to class instance responsible for transmitting and receiving RF messages.
    #else
        #error "Not implemented"
    #endif
    #ifdef CENTRAL_UNIT
        std::unique_ptr<CentralUnitAddressing> mpAddressing; ///< Pointer to CentralUnitAddressing class instance.
    #else
        std::unique_ptr<ModuleAddressing> mpAddressing; ///< Pointer to ModuleAddressing class instance.
    #endif

    typedef enum : uint8_t {
        DEFAULT_STATUS_NOTIF = 0,
        // rf communication timeouts
        BYTE_TIMEOUT_NOTIF,
        MESSAGE_TIMEOUT_NOTIF,
        // suspending notifications
        SUSPEND_DECODE_MESSAGE_TASK_NOTIF,
        SUSPEND_ENCODE_MESSAGE_TASK_NOTIF,
        // ping notifications
        START_PINGING_NOTIF,
        PING_TIMEOUT_NOTIF,
        // addressing notifications
        READ_RAW_MESSAGE_NOTIF,
        STOP_ADDRESSING_ALGORITHM_NOTIF,
    } mCommunicationMainNotifications; ///< Enum with main communication task notifications.

    uint8_t mLastTransmittedMessage[MESSAGE_SIZE]; ///< Recently transmitted message (for "repeat" logic).
    uint8_t mLastTransmittedMessageAttempts = 0; ///< Counter of repeats of last sent message.

    portMUX_TYPE mCriticalSectionMutex = portMUX_INITIALIZER_UNLOCKED; ///< FreeRTOS critical section mutex (used only for resetting encode task).

    SemaphoreHandle_t mLastTransmittedMessageMutex = nullptr; ///< Handle to FreeRTOS mutex protecting the last transmitted message and last transmitted counter.

    QueueHandle_t mMainNotificationsQueue = nullptr; ///< Handle to FreeRTOS queue for notifications for the Main task, queue length: 5 bytes (uint8_t).
    QueueHandle_t mReceiveMessageQueue = nullptr; ///< Handle to FreeRTOS queue for received (decoded and internal) messages, queue length: 10x64 bytes (uint8_t).
    QueueHandle_t mReceiveByteQueue = nullptr; ///< Handle to FreeRTOS queue for bytes to decode get from RF transmission, queue length: 128 bytes (uint8_t).
    QueueHandle_t mSendMessagesQueue = nullptr; ///< Handle to FreeRTOS queue for messages to encode and RF transmission, queue length: 10x64 bytes (uint8_t).

    TaskHandle_t mCommunicationMainTaskHandle = nullptr; ///< Handle to FreeRTOS main communication task.
    TaskHandle_t mDecodeMessageTaskHandle = nullptr; ///< Handle to FreeRTOS decode task.
    TaskHandle_t mEncodeMessageTaskHandle = nullptr; ///< Handle to FreeRTOS encode task.
    TaskHandle_t mSendCustomMessageTaskHandle = nullptr; ///< Handle to FreeRTOS task for handling custom messages sent via Serial.

    TimerHandle_t mReceiveMessageTimeoutTimer = nullptr; ///< Handle to FreeRTOS software timer indicating timeout of the message.
    TimerHandle_t mReceiveByteTimeoutTimer = nullptr; ///< Handle to FreeRTOS software timer indicating timeout of the receiving byte for decode task.
    TimerHandle_t mPingTimeoutTimer = nullptr; ///< Handle to FreeRTOS software timer indicating timeout of "ping" message.

    std::shared_ptr<ul::Logger> mpLogger;
};