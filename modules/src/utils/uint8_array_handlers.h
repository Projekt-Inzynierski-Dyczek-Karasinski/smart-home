#pragma once
#include <Arduino.h>

// TODO add @example tags


namespace Utils {
    /**
     * @brief Namespace containing utility functions for uint8_t arrays.
     * @note Available functions:
     * void printArrayAsChar()
     * void prepareBuffer()
     * uint8_t calcLenOfDataInArray()
     * bool areArraysEqual()
     */
    namespace ArrayHandlers {
        /**
         * @brief Prints given uint8_t array as a char array.
         * @param array Array to be printed.
         * @param len Length of the array.
         */
        void printArrayAsChar(const uint8_t *array, uint8_t len);

        /**
         * @brief Prints given uint8_t array as a int array.
         * @param array Array to be printed.
         * @param len Length of the array.
         */
        void printArrayAsInt(const uint8_t *array, uint8_t len);

        /**
         * @brief Fills given uint8_t array with 0.
         * @param buffer Array to be filled with 0.
         * @param len Length of the value array.
         */
        void clearBuffer(uint8_t *buffer, uint8_t len);

        /**
         * @brief Sets given uint8_t array (buffer) to given value and fills the rest with 0.
         * @param buffer Array to be filled with given value.
         * @param value Array to be copied into buffer.
         * @param len Length of the value array.
         * @param maxLen Length of the buffer.
         */
        void prepareBuffer(uint8_t *buffer, const uint8_t *value, uint8_t len, uint8_t maxLen);

        /**
         * @brief Calculates length of data in given uint8_t array (assuming that first occur of 0 is end of data).
         * @param array Array to be calculated.
         * @param maxLen Length of the array.
         * @return Length of the array until first occur of 0 or maxLen if no 0 was found.
         */
        uint8_t calcLenOfDataInArray(const uint8_t *array, uint8_t maxLen);
        /**
         * @brief Calculates length of data in given char array (assuming that first occur of '\0' is end of data).
         * @param array Array to be calculated.
         * @param maxLen Length of the array.
         * @return Length of the array until first occur of '\0' or maxLen if no '\0' was found.
         */
        uint8_t calcLenOfDataInArray(const char *array, uint8_t maxLen);

        /**
         * @brief Compares two uint8_t arrays of given length.
         * @param array1 First uint8_t array to be compared.
         * @param array2 Second uint8_t array to be compared.
         * @param len Length of the arrays.
         * @return True if arrays are equal, false otherwise.
         */
        bool areArraysEqual(const uint8_t *array1, const uint8_t *array2, uint8_t len);
    }
}
