#pragma once

#define RF_CHANNELS
#define DEFAULT_CHANNEL 1
#define MAX_CHANNEL 4 // TODO change back to 127 (saving flash memory)
// #define MAX_CHANNEL 127

#define SETUP_COMMAND_SIZE 10
#define SETUP_MAX_NUM_OF_COMMANDS 5
#define SETUP_MAX_LEN_OF_RESPONSE 43

#define DELAY_AFTER_SET_PIN_LOW 40 // from HC12 documentation 
#define DELAY_AFTER_SET_PIN_HIGH 80 // from HC12 documentation

#define FIRST_SETUP_SEMAPHORE_TIMEOUT 100 // 0.1s