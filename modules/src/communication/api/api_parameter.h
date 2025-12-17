#pragma once

#include <Arduino.h>
#include <vector>

#include "special_byte/special_byte_parameter.h"

namespace Comms::API {
     template<typename T>
     class APIParameter {
     public:
         explicit APIParameter(T parameter, bool isError = false);
         explicit APIParameter(const uint8_t *parameter);

         T getValue();

         uint8_t getBytesRepresentationLength() const;

         void getBytesRepresentation(uint8_t *array) const;

         SpecialByteParameter getSpecialByte() const;

     private:
         void customHton(T input);
         void customNtoh(const uint8_t* input);

         T mValue;
         uint8_t mBytesRepresentation[sizeof(T) + 1];
    };

    template<typename T>
    class APIParameter<T*> {
    public:
        APIParameter(const T* parameter, size_t length);
        explicit APIParameter(const uint8_t *parameter);

        void getValue(T* outputArray);

        uint8_t getBytesRepresentationLength() const;
        uint8_t getValueLength() const;

        void getBytesRepresentation(uint8_t *outputArray);

        SpecialByteParameter getSpecialByte() const;

    private:
        std::vector<uint8_t> mBytesRepresentation;
    };

     // Uint / error
     template class APIParameter<uint8_t>;
     // Uint
     template class APIParameter<uint16_t>;
     template class APIParameter<uint32_t>;
     template class APIParameter<uint64_t>;

     // int
     template class APIParameter<int8_t>;
     template class APIParameter<int16_t>;
     template class APIParameter<int32_t>;
     template class APIParameter<int64_t>;

     // float
     template class APIParameter<float>;
     template class APIParameter<double>;

     // ASCII
     template class APIParameter<char*>;
     // raw
     template class APIParameter<uint8_t*>;

    using APIParameterVariant = std::variant<
        APIParameter<uint8_t>,
        APIParameter<uint16_t>,
        APIParameter<uint32_t>,
        APIParameter<uint64_t>,
        APIParameter<int8_t>,
        APIParameter<int16_t>,
        APIParameter<int32_t>,
        APIParameter<int64_t>,
        APIParameter<float>,
        APIParameter<double>,
        APIParameter<char*>,
        APIParameter<uint8_t*>
    >;
}
