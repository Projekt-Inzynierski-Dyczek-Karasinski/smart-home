#include <memory>

#include "pairing_button.h"
#include "utils/logger.h"

#define DEBOUNCING_TIME 100
#define DEBOUNCING_COUNTER_TO_SECONDS(value) (value * DEBOUNCING_TIME / 1000)

PairingButton* PairingButton::mspInstance = nullptr;
Comms::Communication* PairingButton::mspCommunication = nullptr;
DebugLED* PairingButton::mspDebugLED = nullptr;

uint8_t PairingButton::msButtonMode = 0;
uint8_t PairingButton::msButtonPressCounter = 0;
int8_t PairingButton::msButtonNotPressedCounter = 3;
TimerHandle_t PairingButton::msButtonPressTimer = nullptr;


PairingButton* PairingButton::getInstance(DebugLED *debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger) {
    if (mspInstance == nullptr) {
        mspInstance = new PairingButton(debugLED, communication, logger);
    }
    return mspInstance;
}

PairingButton::PairingButton(DebugLED *debugLED, Comms::Communication *communication, const std::shared_ptr<ul::Logger> &logger) {
    mspDebugLED = debugLED;
    mspCommunication = communication;
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

    mpLogger = logger;
    mpLogger->info("PairingButton Class", "PairingButton initialized.");
}

PairingButton::~PairingButton() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    deleteButtonPressTimer();
}

void IRAM_ATTR PairingButton::buttonISR() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    mspInstance->mpLogger->debug("PairingButton ISR", "Button pressed.");
    startButtonPressTimer();

    // Force context switch if higher priority task was woken
    constexpr BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


// ====================== Button Press Timer ======================

void PairingButton::buttonPressTimerCallback() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        msButtonPressCounter++;
        msButtonNotPressedCounter = 3;
        
        // after pressing button for 10 seconds call createResetBlinkTask()
        if (DEBOUNCING_COUNTER_TO_SECONDS(msButtonPressCounter) >= 10 && msButtonMode != 2) {
            msButtonMode = 2;
            mspDebugLED->createResetBlinkTask();
            
        } 
        // after pressing button for 3 seconds call createPairingBlinkTask()
        else if (DEBOUNCING_COUNTER_TO_SECONDS(msButtonPressCounter) >= 3 && msButtonMode == 0) {
            if (msButtonMode != 1) {
                mspCommunication->startAddressingAlgorithm();
            }
            msButtonMode = 1;
        }
    } else {
        msButtonNotPressedCounter--;

        // if button will not be press for 3*DEBOUNCING_TIME (0.3 seconds) timer will stop
        if (msButtonNotPressedCounter <= 0) {
            deleteButtonPressTimer();     
            msButtonMode = 0;
            attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);       
        }
    }
}
void PairingButton::buttonPressTimerCallbackHandle(TimerHandle_t xTimer) {
    const PairingButton* instance = static_cast<PairingButton*>(pvTimerGetTimerID(xTimer)); 
    instance->buttonPressTimerCallback();
}
void PairingButton::startButtonPressTimer() {
    if (msButtonPressTimer == nullptr) {
        msButtonPressTimer = xTimerCreate(
            "Button Press Timer",
            pdMS_TO_TICKS(DEBOUNCING_TIME),
            pdTRUE,
            nullptr,
            buttonPressTimerCallbackHandle
        );
    }
    if (msButtonPressTimer != nullptr) {
        xTimerStart(msButtonPressTimer, portMAX_DELAY);
    }
}
void PairingButton::deleteButtonPressTimer() {
    if (msButtonPressTimer != nullptr) {
        xTimerDelete(msButtonPressTimer, portMAX_DELAY);
        msButtonPressTimer = nullptr;
    } 
    msButtonPressCounter = 0;
    msButtonNotPressedCounter = 3;
}
// ================================================================