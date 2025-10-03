#pragma once

#include "common/smart_home_config.h"

// Logger
#ifndef TERMINAL_BAUD_RATE
    #define TERMINAL_BAUD_RATE 9600
#endif

#ifndef LOGGING_LEVEL
    #ifdef DEBUG_MODE
        #define LOGGING_LEVEL 3 // INFO
    #else
        #define LOGGING_LEVEL 0 // NONE
    #endif
#endif