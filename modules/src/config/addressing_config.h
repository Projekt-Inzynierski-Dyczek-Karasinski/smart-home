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

// ===================== Addressing API Calls =====================

#define ADDRESSING_ABORT "ADabor"
#define ADDRESSING_RESTART "ADrest"
#define ADDRESSING_NC_REAL_MAC_RF_CHANNELS "ADncry"
#define ADDRESSING_NC_REAL_MAC_NO_RF_CHANNELS "ADncrn"
#define ADDRESSING_NC_FAKE_MAC_RF_CHANNELS "ADncfy"
#define ADDRESSING_NC_FAKE_MAC_NO_RF_CHANNELS "ADncfn"
#define ADDRESSING_NEW_IP_NEW_RF_CHANNEL "ADi?c?"
#define ADDRESSING_NEW_IP_NEW_NO_RF_CHANNEL "ADi?\0\0"
#define ADDRESSING_SUMMARY "ADsumm"
#define ADDRESSING_SUMMARY_OK "ADsuok"
#define ADDRESSING_SUMMARY_BAD "ADsuba"
#define ADDRESSING_PING "ADping"
#define ADDRESSING_REPING "ADrepi"
// ================================================================