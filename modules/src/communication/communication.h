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
    // TODO remove "testHC12" methods
    static void testHC12Task();
    static void createTestHC12TaskHandle(void *parameters);
    static void createTestHC12Task();
    static void deleteTestHC12Task();
    static TaskHandle_t msTestHC12TaskHandle;

    // TODO remove "sendCustomMessage" methods
    static void sendCustomMessageTask();
    static void createSendCustomMessageTaskHandle(void *parameters);
    static void createSendCustomMessageTask();
    static void deleteSendCustomMessageTask();
    static TaskHandle_t msSendCustomMessageTaskHandle;

    static void receiveMessageTask();
    static void createReceiveMessageTaskHandle(void *parameters);
    static void createReceiveMessageTask();
    static void deleteReceiveMessageTask();

    static void printMessageTask();
    static void createPrintMessageTaskHandle(void *parameters);
    static void createPrintMessageTask();
    static void deletePrintMessageTask();
    static TaskHandle_t msPrintMessageTaskHandle;

    // TODO remove "msReceiveMessageBuffer"
    // static void createReceiveMessageBuffer();
    // static void deleteReceiveMessageBuffer();

    static void createReceiveMessageQueue();
    static void deleteReceiveMessageQueue();
    
    static TaskHandle_t msReceiveMessageTaskHandle;
    // TODO remove "msReceiveMessageBuffer"
    // static MessageBufferHandle_t msReceiveMessageBuffer;
    static QueueHandle_t msReceiveMessageQueue;

    static HardwareSerial *mspSerial;
    static char msMACAddress[12];
};

#endif