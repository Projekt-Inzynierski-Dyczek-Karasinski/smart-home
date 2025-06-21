#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "universal_module_system/debug_led.h"


class Communication {
public:
    Communication(DebugLED *debugLED);
    ~Communication();

    static void startAddresingAlgorithm();

private:
    static void createCommunicationQueues();
    static void deleteCommunicationQueues();


    static void receiveMessageTask();
    static void createReceiveMessageTaskHandle(void *parameters);
    static void createReceiveMessageTask();
    static void deleteReceiveMessageTask();
    
    static void readHC12HandlerTask();
    static void createReadHC12HandlerTaskHandle(void *parameters);
    static void createReadHC12HandlerTask();
    static void deleteReadHC12HandlerTask();

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

    // TODO remove printMessageTask
    static void printMessageTask();
    static void createPrintMessageTaskHandle(void *parameters);
    static void createPrintMessageTask();
    static void deletePrintMessageTask();
    

    static void receiveMessageTimeoutTimerCallback();
    static void receiveMessageTimeoutTimerCallbackHandle(TimerHandle_t xTimer);
    static void receiveByteTimeoutTimerCallback();
    static void receiveByteTimeoutTimerCallbackHandle(TimerHandle_t xTimer);
    static void createReceiveTimers();
    static void deleteReceiveTimers();
    
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
        defaultStatus = 0,
        sendingTaskWaiting = 1,
        byteTimeout = 2,
        messageTimeout = 3
    } mReadHC12NotificationStatus;

    // typedef enum : uint32_t {
    //     defaultStatus = 0,
    //     addressingTimeout = 1,
    // } mAddressingNotificationStatus;
    // typedef Communication::TimeoutStatus TimeoutStatus_t;

    static QueueHandle_t msReceiveMessageQueue;
    static QueueHandle_t msReceiveByteQueue;
    static QueueHandle_t msSendMessagesQueue;

    static TaskHandle_t msReceiveMessageTaskHandle;
    static TaskHandle_t msReadHC12HandlerTaskHandle;
    // TODO remove "sendCustomMessage" methods
    static TaskHandle_t msSendCustomMessageTaskHandle;
    static TaskHandle_t msSendMessageTaskHandle;
    static TaskHandle_t msAddressingTaskHandle;
    // TODO remove printMessageTask
    static TaskHandle_t msPrintMessageTaskHandle;

    static TimerHandle_t msReceiveMessageTimeoutTimer;
    static TimerHandle_t msReceiveByteTimeoutTimer;
    static TimerHandle_t msAddressingTimeoutTimer;
};

#endif