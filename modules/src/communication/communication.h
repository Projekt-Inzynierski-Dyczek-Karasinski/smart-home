#pragma once

#include "../../config/system_config/communication_config.h"

#include "universal_module_system/debug_led.h"

#include "connection/connection.h"
#ifdef HC12_MODULE
    #include "communication/hc12.h"
#endif
#ifdef CENTRAL_UNIT
    #include "communication/addressing/central_unit_addressing.h"
#else
    #include "communication/addressing/module_addressing.h"
#endif


namespace ul = Utils::Logging;
namespace ums = UniversalModuleSystem;

namespace Comms {
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
         * @param debugLED Shared pointer to DebugLED instance.
         * @param logger Shared pointer to the Logger instance.
         * @return Reference to the singleton Communication instance.
         */
        static Communication& getInstance(
            const std::shared_ptr<ums::DebugLED> &debugLED = nullptr,
            const std::shared_ptr<ul::Logger> &logger = nullptr
        );

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

        /**
         * @brief Signals the main task to delete and recreate encode task (used after getting new IP address).
         */
        void resetEncodeMessageTask();

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
         * @brief Add a message that needs to be transmitted to the <b>front of</b> the encoding queue.
         * @details This method works same as <code>sendMessage()</code>,
         *          but adds message to the front of the encoding queue,
         *          making sure that passed message will be sent first.
         * @param message Message to transmit.
         */
        void sendPriorityMessage(const uint8_t message[MESSAGE_SIZE]) const;

        /**
         * @brief Add a message to received queue (omitting decoding process).
         *        Used for internal communication of different parts of module.
         * @param message Message.
         */
        void sendInternalMessage(const uint8_t message[MESSAGE_SIZE]) const;

        /**
         * @brief Call rf module's method <code>firstChangeRFChannel()</code>.
         * @param channel Channel to change.
         */
        void changeRFChannel(uint8_t channel) const;

        /**
         * @brief Method used for safe disabling rf module.
         */
        void waitAndDisableRfModule() const;

        /**
         * @brief Puts the rf module to sleep.
         * @details Calls rf module's <code>sleep()</code> method.
         */
        void putRfModuleToSleep() const;

        /**
         * @brief Sends "end" message and ends connection.
         */
        void endConnection() const;

        /**
         * @brief Gets default RF channel.
         * @return Default RF channel.
         */
        uint8_t getDefaultRfChannel() const;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         *        Initializes FreeRTOS queues, tasks, timers, semaphores necessary for all communication (internal and RF) works.
         * @param debugLED Shared pointer to DebugLED instance.
         * @param logger Shared pointer to the Logger instance.
         */
        explicit Communication(const std::shared_ptr<ums::DebugLED> &debugLED, const std::shared_ptr<ul::Logger> &logger);
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
         */
        void receivedMessageDecider();
        /**
         * @brief Handles when main task is in default state.
         */
        void normalOperationHandling();
        /**
         * @brief Main FreeRTOS task for Communication class.
         * It is responsible for suspending/deleting resuming/creating other communication related tasks.
         * Coordinates work of different parts of system.
         * Decide (using receivedMessageDecider() method) what to do with incoming messages.
         * Handles notifications from other tasks/timers.
         * @param parameters Task parameters.
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
        bool isCheckSumCorrect(const uint8_t message[PROTOCOL_SIZE]) const;

        /**
         * @brief Extracts a message from protocolBuffer and saves it to given buffer. Checks for packet loss.
         * @param protocolBuffer Buffer with raw message.
         * @param messageBuffer Buffer to save extracted message.
         * @return True if successfully extracts a message (without packet loss), false otherwise.
         */
        bool extractMessageFromProtocolBuffer(const uint8_t protocolBuffer[][PROTOCOL_SIZE], uint8_t *messageBuffer) const;

        /**
         * @brief FreeRTOS task for decoding incoming messages from the byte queue.
         * @param parameters Task parameters.
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
         * @param parameters Task parameters.
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
         * @param parameters Pointer to parameters given by FreeRTOS.
         * @note This task runs continuously in the background, but with big delay in main loop to minimize impact to other background tasks.
         */
        static void terminalInputTask(void *parameters);
        /**
         * @brief Create the terminal input FreeRTOS task.
         */
        void createTerminalInputTask();
        /**
         * @brief Delete the terminal input FreeRTOS task.
         */
        void deleteTerminalInputTask();

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

        std::shared_ptr<ums::DebugLED> mpDebugLED; ///< Pointer to debugLED class instance.
        std::shared_ptr<ul::Logger> mpLogger;
        Connection *mpConnection; ///< Pointer to Connection class instance.

        #ifdef HC12_MODULE
            std::shared_ptr<HC12> mpRfModule; ///< Pointer to class instance responsible for transmitting and receiving RF messages.
        #else
            #error "Not implemented"
        #endif

        #ifdef CENTRAL_UNIT
            std::shared_ptr<CentralUnitAddressing> mpAddressing; ///< Pointer to CentralUnitAddressing class instance.
        #else
            std::shared_ptr<ModuleAddressing> mpAddressing; ///< Pointer to ModuleAddressing class instance.
        #endif

        typedef enum : uint8_t {
            DEFAULT_STATUS_NOTIF = 0,
            // rf communication timeouts
            BYTE_TIMEOUT_NOTIF,
            MESSAGE_TIMEOUT_NOTIF,
            // suspending notifications
            SUSPEND_DECODE_MESSAGE_TASK_NOTIF,
            SUSPEND_ENCODE_MESSAGE_TASK_NOTIF,
            // addressing notifications
            READ_RAW_MESSAGE_NOTIF,
            STOP_ADDRESSING_ALGORITHM_NOTIF,
        } mCommunicationMainNotifications; ///< Enum with main communication task notifications.

        portMUX_TYPE mCriticalSectionMutex = portMUX_INITIALIZER_UNLOCKED; ///< FreeRTOS critical section mutex (used only for resetting encode task).

        QueueHandle_t mMainNotificationsQueue = nullptr; ///< Handle to FreeRTOS queue for notifications for the Main task, queue length: 5 bytes (uint8_t).
        QueueHandle_t mReceiveMessageQueue = nullptr; ///< Handle to FreeRTOS queue for received (decoded and internal) messages, queue length: 10x64 bytes (uint8_t).
        QueueHandle_t mReceiveByteQueue = nullptr; ///< Handle to FreeRTOS queue for bytes to decode get from RF transmission, queue length: 128 bytes (uint8_t).
        QueueHandle_t mEncodeMessagesQueue = nullptr; ///< Handle to FreeRTOS queue for messages to encode and RF transmission, queue length: 10x64 bytes (uint8_t).

        TaskHandle_t mCommunicationMainTaskHandle = nullptr; ///< Handle to FreeRTOS main communication task.
        TaskHandle_t mDecodeMessageTaskHandle = nullptr; ///< Handle to FreeRTOS decode task.
        TaskHandle_t mEncodeMessageTaskHandle = nullptr; ///< Handle to FreeRTOS encode task.
        TaskHandle_t mTerminalInputTaskHandle = nullptr; ///< Handle to FreeRTOS task for handling custom messages sent via Serial.

        TimerHandle_t mReceiveMessageTimeoutTimer = nullptr; ///< Handle to FreeRTOS software timer indicating timeout of the message.
        TimerHandle_t mReceiveByteTimeoutTimer = nullptr; ///< Handle to FreeRTOS software timer indicating timeout of the receiving byte for decode task.
    };
}