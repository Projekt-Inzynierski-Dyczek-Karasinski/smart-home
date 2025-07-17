#pragma once

#ifdef HC12_MODULE
    #define BAUD_RATE 9600
    #define RX_PIN 25 
    #define TX_PIN 33
    #define SET_PIN 32
    #define HARDWARE_SERIAL_UART_NR 2
    #define RF_CHANNELS
    
    #define SETUP_COMMAND_SIZE 10
    #define SETUP_MAX_NUM_OF_COMMANDS 5
    #define SETUP_MAX_LEN_OF_RESPONSE 43
#endif

#define LED_PIN 26
#define BUTTON_PIN 27

// #define CENTRAL_UNIT
