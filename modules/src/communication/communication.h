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

    static bool isAbortAddressing(uint8_t *message);
    static void sendAbortMessage();
    static void abortAddressing();
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
    
    static void addressingTimersCallbacks(TimerHandle_t xTimer);
    static void createAddresingTimers();
    static void deleteAddresingTimers();

    static void setupHC12Task(void *parameters);
    // static void createSetupHC12TaskHandle(void *parameters);
    static void createSetupHC12Task();
    static void deleteSetupHC12Task();
    
    static void repeatMessage();
    static void setLastMessage(uint8_t *message, uint8_t size);
    static void resetLastMessage();
    static bool isRepeatMessage(uint8_t *message, uint8_t size);
    static bool isProperMACAndIP(uint8_t *mac, uint8_t ip);

    typedef enum : uint32_t {
        defaultStatusNotif = 0,
        // hc12 timeouts
        byteTimeoutNotif,
        messageTimeoutNotif,
        // sending task
        sendingTaskWaitingNotif,
        // suspending notifications
        suspendReceiveMessageTaskNotif,
        suspendSendMessageTaskNotif,
        // addressing notifications
        readRawMessageNotif,
        createAddressingTaskNotif,
        deleteAddressingTaskNotif,
        abortAddressingNotif,
        abortAddressingWithAbortMessageNotif,
        createSetupHC12TaskNotif,
        deleteSetupHC12TaskNotif,
    } mCommunicationMainNotifications;


    static portMUX_TYPE msCriticalSectionMutex;

    static uint8_t msMACAddress[6];
    static HardwareSerial *mspSerial;
    static DebugLED *mspDebugLED;

    static uint8_t msLastMessage[64]; // MESSAGE_SIZE

    static SemaphoreHandle_t msLastMessageMutex;



    static QueueHandle_t msReceiveMessageQueue;
    static QueueHandle_t msReceiveByteQueue;
    static QueueHandle_t msSendMessagesQueue;
    
    static TaskHandle_t msCommunicationMainTaskHandle;
    static TaskHandle_t msReceiveMessageTaskHandle;
    // TODO remove "sendCustomMessage" methods
    static TaskHandle_t msSendCustomMessageTaskHandle;
    static TaskHandle_t msSendMessageTaskHandle;
    static TaskHandle_t msAddressingTaskHandle;
    static TaskHandle_t msSetupHC12TaskHandle;

    static TimerHandle_t msReceiveMessageTimeoutTimer;
    static TimerHandle_t msReceiveByteTimeoutTimer;
    static TimerHandle_t msSuspendReceiveMessageTimer;
    static TimerHandle_t msSuspendSendMessageTimer;
    
    static TimerHandle_t msAddressingAbsoluteTimeoutTimer;
    static TimerHandle_t msAddressingMessageTimeoutTimer;

    // IP = 0 unassigned IP
    // IP = 1 central unit's IP
    #ifdef CENTRAL_UNIT
        struct Routing {
            uint8_t MACAddress[6];
            uint8_t IPAddress;
            bool isMACReal;
        };
        static struct Routing msRoutingTable[255];
    #else
        static SemaphoreHandle_t msAddressDataMutex;
        static uint8_t msCentralUnitMACAddress[6];
        static uint8_t msIPAddress;
    #endif
};

#endif