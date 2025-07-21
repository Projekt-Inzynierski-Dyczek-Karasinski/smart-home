#pragma once

#include <memory>
#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"
#include "universal_module_system/debug_led.h"
#ifdef HC12_MODULE
    #include "communication/hc12.h"
#endif

class Communication {
public:
    static Communication& getInstance(DebugLED *debugLED);
    
    // Delete copy constructor and assignment operator
    Communication(const Communication&) = delete;
    Communication& operator = (const Communication&) = delete;

    static void startAddresingAlgorithm();
    void addByteToDecode(const uint8_t DATA);

private:
    Communication(DebugLED *debugLED);
    ~Communication();

    void createCommunicationQueues();
    void deleteCommunicationQueues();

    static void communicationMainTask(void* parameters);
    void createCommunicationMainTask();
    void deleteCommunicationMainTask();

    // TODO change name for task (decode?)
    static void decodeMessageTask(void *parameters);
    void createDecodeMessageTask();
    void deleteDecodeMessageTask();

    // TODO change name for task (encode?)
    static void encodeMessageTask(void *parameters);
    void createEncodeMessageTask();
    void deleteEncodeMessageTask();

    static void sendCustomMessageTask(void *parameters);
    void createSendCustomMessageTask();
    void deleteSendCustomMessageTask();

    static void communicationTimersCallbacks(TimerHandle_t xTimer);
    void createCommunicationTimers();
    void deleteCommunicationTimers();

    void setLastTransmittedMessage();
    void setLastTransmittedMessage(const uint8_t MESSAGE[MESSAGE_SIZE]);
    void repeatLastTransmittedMessage();
    void transmitRepeatMessage();

    static DebugLED *mspDebugLED;

    #ifdef HC12_MODULE
        std::unique_ptr<HC12> mRfModule;
    #else
        #error "Not implemented"
    #endif

    typedef enum : uint32_t {
        defaultStatusNotif = 0,
        // rf communication timeouts
        byteTimeoutNotif,
        messageTimeoutNotif,
        // sending task
        sendingTaskWaitingNotif,
        // suspending notifications
        suspendDecodeMessageTaskNotif,
        suspendEndcodeMessageTaskNotif,

        readRawMessageNotif,
    } mCommunicationMainNotifications;

    uint8_t mMACAddress[6];
    bool mIsMacAddressReal;

    uint8_t mLastTransmittedMessage[MESSAGE_SIZE];

    SemaphoreHandle_t mLastTransmittedMessageMutex = NULL;

    QueueHandle_t mReceiveMessageQueue = NULL;
    QueueHandle_t mReceiveByteQueue = NULL;
    QueueHandle_t mSendMessagesQueue = NULL;

    TaskHandle_t mCommunicationMainTaskHandle = NULL;
    TaskHandle_t mDecodeMessageTaskHandle = NULL;
    TaskHandle_t mEncodeMessageTaskHandle = NULL;
    TaskHandle_t mSendCustomMessageTaskHandle = NULL;

    TimerHandle_t mReceiveMessageTimeoutTimer = NULL;
    TimerHandle_t mReceiveByteTimeoutTimer = NULL;
};