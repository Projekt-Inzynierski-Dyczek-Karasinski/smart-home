#include "communication/uint8_array_handlers.h"

namespace uint8ArrayHandlers {
    void printArrayAsChar(const uint8_t *array, const uint8_t len) {
        for (uint8_t i = 0; i < len; i++) {
            Serial.print((char)array[i]);
        }
        Serial.println();
    } 
    
    void printArrayAsInt(const uint8_t *array, const uint8_t len) {
        for (uint8_t i = 0; i < len; i++) {
            Serial.print((int)array[i]);
            Serial.print(' ');
        }
        Serial.println();
    } 

    void prepareBuffor(uint8_t *buffor, const uint8_t len) {
        for (uint8_t i = 0; i < len; i++) {
            buffor[i] = 0;
        }
    } 

    void prepareBuffor(uint8_t *buffor, const uint8_t *value, uint8_t len, const uint8_t maxLen) {
        if (len > maxLen) {
            len = maxLen;
            Serial.println("VALUE ERROR! In uint8ArrayHandlers::prepareBuffor() -> len must not be larger than maxLen");
        }

        for (uint8_t i = 0; i < len; i++) {
            buffor[i] = value[i];
        }
        for (uint8_t i = len; i < maxLen; i++) {
            buffor[i] = 0;
        }
    } 

    uint8_t calcLenOfDataInArray(const uint8_t *array, const uint8_t maxLen) {
        for (uint8_t i = 0; i < maxLen; i++) {
            if (array[i] == 0) return i; 
        }
        return maxLen;
    }

    bool areArraysEqual(const uint8_t *array1, const uint8_t *array2, uint8_t len) {
        for (uint8_t i = 0; i < len; i++){
            if (array1[i] != array2[i]) {
                return false;
            }
        }
        return true;
    }
}