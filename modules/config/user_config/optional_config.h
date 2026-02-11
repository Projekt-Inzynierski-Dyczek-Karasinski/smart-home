#pragma once
// TODO !pr check/add comments
// This is optional config data, that data may be set.


// uncomment to enable serial communication (automatically sets LOGGING_LEVEL to INFO if not overwritten below)
// #ifndef DEBUG_MODE
// #define DEBUG_MODE
// #endif

// uncomment to override serial baudrate, default: 115200
// WARNING: DEBUG_MODE must be defined to enable serial communication
// #define TERMINAL_BAUD_RATE 115200

/*
uncomment to override default LOGGING_LEVEL
NONE:    0 - default if DEBUG_MODE is not defined
ERROR:   1
WARNING: 2
INFO:    3 - default if DEBUG_MODE is defined
VERBOSE: 4
DEBUG:   5 - it is not recommended to set LOGGING_LEVEL to DEBUG for entire program
*/
// #define LOGGING_LEVEL 3 // INFO
