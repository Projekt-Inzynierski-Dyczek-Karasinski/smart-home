#ifndef HC12_H
#define HC12_H


#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "config/communication_config.h"

class HC12 {
public:
    static HC12& getInstance();
    
    // Delete copy constructor and assignment operator
    HC12(const HC12&) = delete;
    HC12& operator = (const HC12&) = delete;

    void send(const uint8_t *MESSAGE);
    void setupHC12(const uint8_t *COMMAND);

private:
    HC12();
    ~HC12();

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

    void prepareCommandBuffor(uint8_t *buffor, const uint8_t *value, uint8_t len);
    void printUint8Array(const uint8_t *array, const uint8_t len);
    uint8_t calcLenOfUint8Array(const uint8_t *array, const uint8_t maxLen);

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
#endif