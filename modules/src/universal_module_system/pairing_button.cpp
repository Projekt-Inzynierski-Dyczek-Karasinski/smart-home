#include "pairing_button.h"
#include "smart_home_config.h"

#define DEBOUNCING_TIME 100
// #define DEBOUNCING_COUNTER_TO_SECONDS(value) (value * DEBOUNCING_TIME / 1000)
#define DEBOUNCING_COUNTER_TO_SECONDS(value) (value / 10)

DebugLED* PairingButton::mspDebugLED;

uint8_t PairingButton::msButtonMode = 0;
uint8_t PairingButton::msButtonPressCounter = 0;
int8_t PairingButton::msButtonNotPressedCounter = 3;
TimerHandle_t PairingButton::msButtonPressTimer = NULL;

PairingButton::PairingButton(DebugLED *debugLED) {
    mspDebugLED = debugLED;
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
}

void IRAM_ATTR PairingButton::buttonISR() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    
    startButtonPressTimer();

    // Force context switch if higher priority task was woken
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ---- buttonPressTimer ----
void PairingButton::buttonPressTimerCallback() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        msButtonPressCounter++;
        msButtonNotPressedCounter = 3;
        if (DEBOUNCING_COUNTER_TO_SECONDS(msButtonPressCounter) >= 10 && msButtonMode != 2) {
            msButtonMode = 2;
            mspDebugLED->createResetBlinkTask();
        } else if (DEBOUNCING_COUNTER_TO_SECONDS(msButtonPressCounter) >= 3 && msButtonMode == 0) {
            msButtonMode = 1;
            mspDebugLED->createPairingBlinkTask();
        }
    } else {
        msButtonNotPressedCounter--;
        if (msButtonNotPressedCounter <= 0) {
            deleteButtonPressTimer();     
            msButtonMode = 0;
            attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);       
        }
    }
}
void PairingButton::buttonPressTimerCallbackHandle(TimerHandle_t xTimer) {
    PairingButton* instance = static_cast<PairingButton*>(pvTimerGetTimerID(xTimer));
    instance->buttonPressTimerCallback();
}
void PairingButton::startButtonPressTimer() {
    msButtonPressTimer = xTimerCreate(
        "Button Press Timer",
        pdMS_TO_TICKS(DEBOUNCING_TIME),
        pdTRUE,
        NULL,
        buttonPressTimerCallbackHandle
    );
    xTimerStart(msButtonPressTimer, portMAX_DELAY);
}
void PairingButton::deleteButtonPressTimer() {
    if (msButtonPressTimer != NULL) {
        xTimerDelete(msButtonPressTimer, portMAX_DELAY);
        msButtonPressTimer = NULL;
    } 
    msButtonPressCounter = 0;
    msButtonNotPressedCounter = 3;
}
// ---- buttonPressTimer ----


PairingButton::~PairingButton() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    deleteButtonPressTimer();
}
