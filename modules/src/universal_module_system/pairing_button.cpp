#include "pairing_button.h"
#include "smart_home_config.h"

DebugLED* PairingButton::spDebugLED;

PairingButton::PairingButton(DebugLED *debugLED) {
    spDebugLED = debugLED;
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
}

void IRAM_ATTR PairingButton::buttonISR() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
    Serial.println("Button pressed");
    if (spDebugLED->getConnectionBlinkHandle() == NULL) {
        spDebugLED->createConnectionBlinkTask();}
    // } else {
    //     spDebugLED->deleteConnectionBlinkTask();
    // }

    // Force context switch if higher priority task was woken
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

PairingButton::~PairingButton() {
    detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
}
