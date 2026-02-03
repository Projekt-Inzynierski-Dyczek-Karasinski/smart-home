#pragma once

#include <Arduino.h>
#include <vector>

#include "special_byte/special_byte_parameter.h"

namespace Comms::API {
    /**
    * @brief API parameter wrapper for scalar types.
    * @tparam T Scalar type.
    */
     template<typename T>
     class APIParameter {
     public:
         /**
         * @brief Construct from a scalar value.
         * @param parameter Input scalar value.
         * @param isError If true, encodes SpecialByteParameter as ERROR, default false.
         */
         explicit APIParameter(T parameter, bool isError = false);


         /**
          * @brief Construct from a byte buffer.
          * @details Expects \p parameter[0] to contain the special byte and the next
          * bytes containing the value in big-endian order.
          *
          * @param parameter Pointer to serialized representation.
          * @throws std::invalid_argument If the special byte does not match the expected type.
          */
         explicit APIParameter(const uint8_t *parameter);

         /**
          * @brief Get the stored scalar value in host byte order.
          * @return Current value.
          */
         T getValue();

         /**
          * @brief Get total serialized length in bytes.
          * @return Number of bytes.
          */
         [[nodiscard]] uint8_t getBytesRepresentationLength() const;

         /**
          * @brief Copy the serialized representation into an output buffer.
          * @param array Output buffer.
          */
         void getBytesRepresentation(uint8_t *array) const;

         /**
          * @brief Get the SpecialByteParameter.
          * @return SpecialByteParameter.
          */
         [[nodiscard]] SpecialByteParameter getSpecialByte() const;

     private:
         /**
          * @brief Convert host value to big-endian bytes and store into mBytesRepresentation.
          * @param input Value in host byte order.
          */
         void customHton(T input);

         /**
          * @brief Convert big-endian bytes into host value and store into mValue.
          * @param input Pointer to the payload bytes (does not include special byte).
          */
         void customNtoh(const uint8_t* input);

         T mValue;
         uint8_t mBytesRepresentation[sizeof(T) + 1];
    };


    /**
     * @brief API parameter wrapper specialization for pointer types (arrays/buffers).
     *
     * @tparam T Element type of the pointed-to data (uint8_t for raw bytes, char for ASCII).
     */
    template<typename T>
    class APIParameter<T*> {
    public:
        /**
         * @brief Construct from an array/buffer.
         *
         * @param parameter Pointer to the first element.
         * @param length Number of elements to serialize.
         *
         * @throw std::length_error If given length is too big.
         */
        APIParameter(const T* parameter, size_t length);

        /**
         * @brief Construct from a serialized byte buffer.
         *
         * @param parameter Pointer to serialized representation.
         */
        explicit APIParameter(const uint8_t *parameter);

        /**
        * @brief Copy the payload (without special byte) into an output array.
        * @param outputArray Output buffer.
        */
        void getValue(T* outputArray);

        /**
         * @brief Get total serialized length in bytes.
         * @return Length of mBytesRepresentation.
         */
        [[nodiscard]] uint8_t getBytesRepresentationLength() const;

        /**
         * @brief Get payload length.
         * @return Length of value array.
         */
        [[nodiscard]] uint8_t getValueLength() const;

        /**
         * @brief Copy full serialized representation into an output buffer.
         * @param outputArray Output buffer.
         */
        void getBytesRepresentation(uint8_t *outputArray);

        /**
         * @brief Get the SpecialByteParameter.
         * @return SpecialByteParameter.
         */
        [[nodiscard]] SpecialByteParameter getSpecialByte() const;

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
