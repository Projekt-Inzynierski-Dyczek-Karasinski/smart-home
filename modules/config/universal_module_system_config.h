#pragma once

#include "common/smart_home_config.h"

// DebugLED
#define CONNECTION_BLINK_DELAY 500
#define RESET_BLINK_DELAY 100
#define MAX_RESET_BLINK_TIME 3000

// PairingButton
#define DEBOUNCING_TIME 100
#define DEBOUNCING_COUNTER_TO_SECONDS(value) (value * DEBOUNCING_TIME / 1000)
#define BUTTON_REBOOT_DELAY 1000

// PowerManager
#define AUTO_SLEEP_WAIT_TIME 60000 // 60s