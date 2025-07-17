#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

class Communication; 

class HC12 {
public:
    explicit HC12(Communication *communication);
    ~HC12();

    void send(const uint8_t *MESSAGE);
    void setupHC12(const uint8_t *COMMAND);
    

private:
    // void createQueues();
    // void deleteQueues();
    void createSetupHC12Queues();
    void deleteSetupHC12Queues();
    
    static void HC12MainTask(void *parameters);
    void createHC12MainTask();
    void deleteHC12MainTask();
    
    static void setupHC12Task(void *parameters);
    void createSetupHC12Task();
    void deleteSetupHC12Task();

    static HC12 *mspHC12;
    Communication *mpCommunication;
    HardwareSerial *mpSerial;
    unsigned long mBaudRate;

    typedef enum : uint32_t {
        defaultStatusNotif = 0,
        byteTimeoutNotif,
        // setup HC_12 notifications
        createSetupHC12TaskNotif,
        deleteSetupHC12TaskNotif,
    } mHC12MainNotifications;
    
    QueueHandle_t mSetupHC12CommandsQueue = NULL;
    QueueHandle_t mSetupHC12ReceiveQueue = NULL;

    TaskHandle_t mHC12MainTaskHandle = NULL;
    TaskHandle_t mSetupHC12TaskHandle = NULL;
};