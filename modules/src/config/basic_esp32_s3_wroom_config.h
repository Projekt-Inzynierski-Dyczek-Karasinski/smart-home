#pragma once

#ifdef HC12_MODULE
    #define RF_CHANNELS

    #define BAUD_RATE 9600
    #define RX_PIN 6 
    #define TX_PIN 7
    #define SET_PIN 5
    #define HARDWARE_SERIAL_UART_NR 2
    
    #define SETUP_COMMAND_SIZE 10
    #define SETUP_MAX_NUM_OF_COMMANDS 5
    #define SETUP_MAX_LEN_OF_RESPONSE 43
    #define DELAY_BETWEEN_MESSAGES 25
#endif

#define LED_PIN 39
#define BUTTON_PIN 1
#define CENTRAL_UNIT 

