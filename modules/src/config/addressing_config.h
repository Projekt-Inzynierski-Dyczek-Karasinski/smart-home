#pragma once

#include "smart_home_config.h"

// TODO assign final value
#define ADDRESSING_ABSOLUTE_TIMEOUT 60000 // 60s
// TODO assign final value
#define ADDRESSING_ABSOLUTE_MAX_ATTEMPTS 3
// TODO assign final value
#define ADDRESSING_MESSAGE_TIMEOUT 5000 // 5s
// TODO assign final value
#define ADDRESSING_MAX_ATTEMPTS 5
// TODO assign final value
#define ADDRESSING_NUM_OF_ABORT_MESSAGES 3
// TODO assign final value
#define ADDRESSING_DELAY_BETWEEN_ABORT_MESSAGES 200 // 0.2s
// TODO assign final value
#define ADDRESSING_DELAY_BETWEEN_ATTEMPTS 1000 // 1s

#define NULL_IP 0
#define CENTRAL_UNIT_IP 1
#define MAX_NUM_OF_MODULES 254
#define MAC_ADDRESS_LENGTH 6
#define indexToIP(value) (uint8_t)(value + 2)
#define ipToIndex(value) (uint8_t)(value - 2)
#define rfChannelToIndex(value) (uint8_t)(value - 1)
#define indexToRfChannel(value) (uint8_t)(value + 1)
#ifdef HC12_MODULE
    #include "config/hc12_common_config.h"
#endif

// TODO change API Calls to numeric values
// TODO !BEFORE PULL REQUEST! check if addressing work properly after changing API

// ===================== Addressing API Calls =====================

#define ADDRESSING_ABORT "\0ADabo"
#define ADDRESSING_RESTART "\0ADres"
#define ADDRESSING_NC_REAL_MAC_RF_CHANNELS "\0ADnry"
#define ADDRESSING_NC_REAL_MAC_NO_RF_CHANNELS "\0ADnrn"
#define ADDRESSING_NC_FAKE_MAC_RF_CHANNELS "\0ADnfy"
#define ADDRESSING_NC_FAKE_MAC_NO_RF_CHANNELS "\0ADnfn"
#define ADDRESSING_NEW_IP_NEW_RF_CHANNEL "\0AD?c?"
#define ADDRESSING_NEW_IP_NEW_NO_RF_CHANNEL "\0AD?\0\0"
#define ADDRESSING_SUMMARY "\0ADsum"
#define ADDRESSING_SUMMARY_OK "\0ADsok"
#define ADDRESSING_SUMMARY_BAD "\0ADsba"
#define ADDRESSING_PING "\0ADpin"
#define ADDRESSING_REPING "\0ADrep"
// ================================================================