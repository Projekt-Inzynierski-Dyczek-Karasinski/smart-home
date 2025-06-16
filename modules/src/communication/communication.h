#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <HardwareSerial.h>
// #include <freertos/message_buffer.h>

class Communication {
public:
    Communication();
    ~Communication();

private:
    static void createReceiveMessageQueue();
    static void deleteReceiveMessageQueue();
    static void createSendMessageQueue();
    static void deleteSendMessageQueue();

    // TODO remove "sendCustomMessage" methods
    static void sendCustomMessageTask();
    static void createSendCustomMessageTaskHandle(void *parameters);
    static void createSendCustomMessageTask();
    static void deleteSendCustomMessageTask();

    static void sendMessageTask();
    static void createSendMessageTaskHandle(void *parameters);
    static void createSendMessageTask();
    static void deleteSendMessageTask();

    static void receiveMessageTask();
    static void createReceiveMessageTaskHandle(void *parameters);
    static void createReceiveMessageTask();
    static void deleteReceiveMessageTask();

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
    
    // TODO remove
    static char calculateCheckSum(char *message);
    static bool checkMessage(char *message);


    // enum class TimeoutStatus : uint32_t {
    //     noTimeout = 0,
    //     charTimeout = 1,
    //     messageTimeout = 2
    // };
    typedef enum : uint32_t {
        noTimeout = 0,
        byteTimeout = 1,
        messageTimeout = 2
    } msTimeoutStatus;
    // typedef Communication::TimeoutStatus TimeoutStatus_t;

    static uint8_t msMACAddress[6];
    static HardwareSerial *mspSerial;

    static TaskHandle_t msPrintMessageTaskHandle;
    static TaskHandle_t msReceiveMessageTaskHandle;
    // TODO remove "sendCustomMessage" methods
    static TaskHandle_t msSendCustomMessageTaskHandle;
    static TaskHandle_t msSendMessageTaskHandle;

    static QueueHandle_t msReceiveMessageQueue;
    static QueueHandle_t msSendMessagesQueue;

    static SemaphoreHandle_t msHC12ReceiveMutex;

    static TimerHandle_t msReceiveMessageTimeoutTimer;
    static TimerHandle_t msReceiveByteTimeoutTimer;
};

#endif