#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"


class Communication {
public:
    Communication(DebugLED *debugLED);
    ~Communication();

    static void startAddresingAlgorithm();

private:
    static void communicationMainTask();
    static void createCommunicationMainTaskHandle(void *parameters);
    static void createCommunicationMainTask();
    static void deleteCommunicationMainTask();


    static void createCommunicationQueues();
    static void deleteCommunicationQueues();


    static void receiveMessageTask();
    static void createReceiveMessageTaskHandle(void *parameters);
    static void createReceiveMessageTask();
    static void deleteReceiveMessageTask();
    

    // TODO remove "sendCustomMessage" methods
    static void sendCustomMessageTask();
    static void createSendCustomMessageTaskHandle(void *parameters);
    static void createSendCustomMessageTask();
    static void deleteSendCustomMessageTask();

    static void sendMessageTask();
    static void createSendMessageTaskHandle(void *parameters);
    static void createSendMessageTask();
    static void deleteSendMessageTask();

    static void addressingTask();
    static void createAddressingTaskHandle(void *parameters);
    static void createAddressingTask();
    static void deleteAddressingTask();
    

    // static void messageTimeoutTimerCallback();
    // static void messageTimeoutTimerCallbackHandle(TimerHandle_t xTimer);

    // static void byteTimeoutTimerCallback();
    // static void byteTimeoutTimerCallbackHandle(TimerHandle_t xTimer);

    // static void suspendReceiveMessageTimerCallback();
    // static void suspendReceiveMessageTimerCallbackHandle(TimerHandle_t xTimer);

    // static void suspendSendMessageCallback();
    // static void suspendSendMessageCallbackHandle(TimerHandle_t xTimer);
    static void communicationTimersCallbacks(TimerHandle_t xTimer);
    static void createCommunicationTimers();
    static void deleteCommunicationTimers();
    
    static void addressingTimeoutTimerCallback();
    static void addressingTimeoutTimerCallbackHandle(TimerHandle_t xTimer);
    static void createAddresingTimer();
    static void deleteAddresingTimer();


    // enum class TimeoutStatus : uint32_t {
    //     noTimeout = 0,
    //     charTimeout = 1,
    //     messageTimeout = 2
    // };
    static uint8_t msMACAddress[6];
    static HardwareSerial *mspSerial;
    static DebugLED *mspDebugLED;

    typedef enum : uint32_t {
        defaultStatusNotif = 0,
        sendingTaskWaitingNotif,
        byteTimeoutNotif,
        messageTimeoutNotif,
        readRawMessageNotif,
        suspendReceiveMessageTaskNotif,
        suspendSendMessageTaskNotif,
        createAddressingTaskNotif,
        deleteAddressingTaskNotif,
    } mCommunicationMainNotifications;

    // typedef enum : uint32_t {
    //     defaultStatus = 0,
    //     addressingTimeout = 1,
    // } mAddressingNotificationStatus;
    // typedef Communication::TimeoutStatus TimeoutStatus_t;
    static TaskHandle_t msCommunicationMainTaskHandle;
    

    static QueueHandle_t msReceiveMessageQueue;
    static QueueHandle_t msReceiveByteQueue;
    static QueueHandle_t msSendMessagesQueue;

    static TaskHandle_t msReceiveMessageTaskHandle;
    // TODO remove "sendCustomMessage" methods
    static TaskHandle_t msSendCustomMessageTaskHandle;
    static TaskHandle_t msSendMessageTaskHandle;
    static TaskHandle_t msAddressingTaskHandle;

    static TimerHandle_t msReceiveMessageTimeoutTimer;
    static TimerHandle_t msReceiveByteTimeoutTimer;
    static TimerHandle_t msSuspendReceiveMessageTimer;
    static TimerHandle_t msSuspendSendMessageTimer;
    
    static TimerHandle_t msAddressingTimeoutTimer;

    #ifdef CENTRAL_UNIT
        struct Routing {
            uint8_t MACAddress[6];
            uint8_t IPAddress;
            bool isMACReal;
        };
        static struct Routing msRoutingTable[255];
    #else
        static uint8_t msCentralUnitMACAddress[6];
        static uint8_t msIPAddress;
    #endif
};

#endif