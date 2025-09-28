#include "uint8_array_handlers.h"

namespace Utils {
    namespace ArrayHandlers {
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

        void clearBuffer(uint8_t *buffer, const uint8_t len) {
            for (uint8_t i = 0; i < len; i++) {
                buffer[i] = 0;
            }
        }

        void prepareBuffer(uint8_t *buffer, const uint8_t *value, uint8_t len, const uint8_t maxLen) {
            if (len > maxLen) {
                len = maxLen;
            }

            for (uint8_t i = 0; i < len; i++) {
                buffer[i] = value[i];
            }

            clearBuffer(&buffer[len], (maxLen - len));
        }

        void prepareBuffer(uint8_t *buffer, const char *value, const uint8_t maxLen, size_t len) {
            if (len == 0) len = strlen(value);
            if (len > maxLen) {
                len = maxLen;
            }

            for (size_t i = 0; i < len; i++) {
                buffer[i] = (uint8_t)value[i];
            }

            clearBuffer(&buffer[len], (maxLen - len));
        }

        uint8_t calcLenOfDataInArray(const uint8_t *array, const uint8_t maxLen) {
            for (uint8_t i = 0; i < maxLen; i++) {
                if (array[i] == 0) return i;
            }
            return maxLen;
        }

        uint8_t calcLenOfDataInArray(const char *array, const uint8_t maxLen) {
            for (uint8_t i = 0; i < maxLen; i++) {
                if (array[i] == '\0') return i;
            }
            return maxLen;
        }

        bool areArraysEqual(const uint8_t *array1, const uint8_t *array2, const uint8_t len) {
            for (uint8_t i = 0; i < len; i++){
                if (array1[i] != array2[i]) {
                    return false;
                }
            }
            return true;
        }

        bool areArraysEqual(const uint8_t *uint8Array, const char *charArray, size_t len) {
            if (len == 0) len = strlen(charArray);

            for (size_t i = 0; i < len; i++) {
                if (uint8Array[i] != (uint8_t)charArray[i]) {
                    return false;
                }
            }
            return true;
        }
    }
}
