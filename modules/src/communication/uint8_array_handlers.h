#pragma once
#include <Arduino.h>

/**
 * @brief Namespace containing utility functions for uint8_t arrays.
 * @note Available functions:
 * 
 * void printArray()
 * 
 * void prepareBuffor()
 * 
 * uint8_t calcLenOfDataInArray() 
 * 
 * bool areArraysEqual()
 * 
 */
namespace uint8ArrayHandlers {
    /**
     * @brief Prints given uint8_t array as a string.
     * 
     * @param array uint8_t array to be printed.
     * @param len length of the array.
     * 
     */
    void printArray(const uint8_t *array, const uint8_t len);

    /**
     * @brief Fills given uint8_t array with 0.
     * 
     * @param buffor uint8_t array to be filled with 0.
     * @param len length of the value array.
     * 
     */
    void prepareBuffor(uint8_t *buffor, const uint8_t len);
    /**
     * @brief Sets given uint8_t array (buffor) to given value and fills the rest with 0.
     * 
     * @param buffor uint8_t array to be filled with given value.
     * @param value uint8_t array to be copied into buffor.
     * @param len length of the value array.
     * @param maxLen length of the buffor.
     * 
     */
    void prepareBuffor(uint8_t *buffor, const uint8_t *value, uint8_t len, const uint8_t maxLen);

    /**
     * @brief Calculates length of data in given uint8_t array.
     * 
     * @param array uint8_t array to be calculated.
     * @param len length of the array.
     * 
     * @return length of the array until first occurrence of 0 or len if no 0 was found.
     * 
     */
    uint8_t calcLenOfDataInArray(const uint8_t *array, const uint8_t len);

    /**
     * @brief Compares two uint8_t arrays of given length.
     * 
     * @param array1 first uint8_t array to be compared.
     * @param array2 second uint8_t array to be compared.
     * @param len length of the arrays.
     * 
     * @return true if arrays are equal, false otherwise.
     * 
     */
    bool areArraysEqual(const uint8_t *array1, const uint8_t *array2, uint8_t len);
}