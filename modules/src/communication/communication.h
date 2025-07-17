#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include <memory>

#include "smart_home_config.h"
#include "config/communication_config.h"

#include "universal_module_system/debug_led.h"
#ifdef HC12_MODULE
    #include "communication/hc12.h"
#endif

// class Communication : public std::enable_shared_from_this<Communication>{
class Communication {
public:
    static Communication& getInstance(DebugLED *debugLED);
    
    // Delete copy constructor and assignment operator
    Communication(const Communication&) = delete;
    Communication& operator = (const Communication&) = delete;

    static void startAddresingAlgorithm();

    void test();

private:
    Communication(DebugLED *debugLED);
    ~Communication();

    static void communicationMainTask(void* parameters);
    void createCommunicationMainTask();
    void deleteCommunicationMainTask();

    static void sendCustomMessageTask(void *parameters);
    void createSendCustomMessageTask();
    void deleteSendCustomMessageTask();

    static DebugLED *mspDebugLED;

    #ifdef HC12_MODULE
        // static HC12 *mRfModule;
        std::unique_ptr<HC12> mRfModule;
    #else
        #error "Not implemented"
    #endif

    TaskHandle_t mCommunicationMainTaskHandle = NULL;
    TaskHandle_t mSendCustomMessageTaskHandle = NULL;
};