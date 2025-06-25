#include <Arduino.h>
#include <HardwareSerial.h>

#include "smart_home_config.h"
#include "universal_module_system/debug_led.h"
#include "universal_module_system/pairing_button.h"
#include "communication/communication.h"

// TODO remove
// HardwareSerial mspSerial(HARDWARE_SERIAL_UART_NR);

// const enum addresingStageEnum : uint8_t {
//     newConnection = 0,
//     newRFChannel = 1,
//     changeRFChannel = 2,
//     summation = 3,
//     oldConfiguration = 4
// };
// uint8_t addresingStage = addresingStageEnum::newConnection;
// const uint8_t messages[5][6] = {
//     {'a', 'a', 'a', 'a', 'a', 'a'},
//     {'b', 'b', 'b', 'b', 'b', 'b'},
//     {'c', 'c', 'c', 'c', 'c', 'c'},
//     {'d', 'd', 'd', 'd', 'd', 'd'},
//     {'e', 'e', 'e', 'e', 'e', 'e'}
// };

// class Test{
// public:
//     Test() {
//         // createTimers();
//         // xTimerStart(timer1, portMAX_DELAY);
//         // xTimerStart(timer2, portMAX_DELAY);
//     }

//     static TimerHandle_t timer1;
//     static TimerHandle_t timer2;

//     static void timer1Callback() {
//         Serial.println("timer1");
//     }

//     static void timer2Callback() {
//         Serial.println("timer2");
//     } 

//     static void timersHandle(TimerHandle_t timer) {
//         Test* instance = static_cast<Test*>(pvTimerGetTimerID(timer));
//         if (timer == Test::timer1){
//             Serial.println("timer1");
//         } else if (timer == Test::timer2){
//             Serial.println("timer2");
//         }
//     }

//     static void createTimers() {
//         Test::timer1 = xTimerCreate(
//             "timer test 1",
//             pdMS_TO_TICKS(1000),
//             pdTRUE,
//             NULL,
//             timersHandle
//         );
//         Test::timer2 = xTimerCreate(
//             "timer test 2",
//             pdMS_TO_TICKS(1500),
//             pdTRUE,
//             NULL,
//             timersHandle
//         );
//         xTimerStart(timer1, portMAX_DELAY);
//         xTimerStart(timer2, portMAX_DELAY);
//     }
// };
// TimerHandle_t Test::timer1 = NULL;
// TimerHandle_t Test::timer2 = NULL;
// Test test;
void setup() {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.begin(9600);
    Serial.println(); 
    Serial.println("---FreeRTOS START---");
    
    DebugLED debugLed;
    Communication communication(&debugLed);
    PairingButton pairingButton(&debugLed, &communication);
    
    // TODO remove
    // mspSerial.begin((unsigned long)BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    // test.createTimers();

    Serial.println("---setup() and loop() deleted---");
    vTaskDelete(NULL);
}
// bool flag = true;

void loop() {
    // TODO remove
    // while (Serial.available() > 0) {
    //     mspSerial.write(Serial.read());
    // }
    // while (mspSerial.available() > 0) {
    //     Serial.write(mspSerial.read());
    // }
    // while (addresingStage != addresingStageEnum::summation){

    //         for (int j = 0; j < 6; j++) {
    //             Serial.print((char)messages[addresingStage][j]);
    //         }
    //         Serial.println();
    //         addresingStage++;

    // }
    // vTaskDelay(pdMS_TO_TICKS(5000));
    // if (flag){
    //     xTimerDelete(test.timer1, portMAX_DELAY);
    //     flag = false;
    // }
    
}