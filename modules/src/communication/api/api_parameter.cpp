#include "api_parameter.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include <endian.h>

namespace Comms::API {
    template<typename T>
    APIParameter<T>::APIParameter(const T parameter, const bool isError)
        : mValue(parameter) {
        mBytesRepresentation[0] = SpecialByteParameter(parameter, 0, isError).getSpecialByte();
        customHton(parameter);
    }

    template<typename T>
    APIParameter<T>::APIParameter(const uint8_t *parameter) {
        T tmp;
        const SpecialByteParameter sb1(parameter[0]);
        const SpecialByteParameter sb2(tmp, 0);

        if (sb1.getSpecialByte() != sb2.getSpecialByte() && sb1.getType() != (uint8_t)parametersTypes::ERROR) {
            throw std::invalid_argument("APIParameter() invalid type");
        }

        std::memcpy(mBytesRepresentation, parameter, sizeof(T) + 1);
        customNtoh(&mBytesRepresentation[1]);
    }

    template<typename T>
    T APIParameter<T>::getValue() {
        return mValue;
    }

    template<typename T>
    uint8_t APIParameter<T>::getBytesRepresentationLength() const {
        return sizeof(T) + 1;
    }

    template<typename T>
    void APIParameter<T>::getBytesRepresentation(uint8_t *array) const {
        std::copy_n(mBytesRepresentation, getBytesRepresentationLength(), array);
    }

    template<typename T>
    SpecialByteParameter APIParameter<T>::getSpecialByte() const {
        return SpecialByteParameter(mBytesRepresentation[0]);
    }

    template<typename T>
    void APIParameter<T>::customHton(const T input) {
        switch (sizeof(T)) {
            case 1: std::memcpy(&mBytesRepresentation[1], &input, sizeof(T)); break;
            case 2: {
                uint16_t tmp;
                std::memcpy(&tmp, &input, sizeof(T));
                tmp = htobe16(tmp);
                std::memcpy(&mBytesRepresentation[1], &tmp, sizeof(T));
                break;
            }
            case 4: {
                uint32_t tmp;
                std::memcpy(&tmp, &input, sizeof(T));
                tmp = htobe32(tmp);
                std::memcpy(&mBytesRepresentation[1], &tmp, sizeof(T));
                break;
            }
            case 8: {
                uint64_t tmp;
                std::memcpy(&tmp, &input, sizeof(T));
                tmp = htobe64(tmp);
                std::memcpy(&mBytesRepresentation[1], &tmp, sizeof(T));
                break;
            }
            default:
                throw std::invalid_argument("hton invalid type");
        }
    }

    template<typename T>
    void APIParameter<T>::customNtoh(const uint8_t* input) {
        switch (sizeof(T)) {
            case 1: std::memcpy(&mValue, input, sizeof(T)); break;
            case 2: {
                uint16_t tmp;
                std::memcpy(&tmp, input, sizeof(T));
                tmp = be16toh(tmp);
                std::memcpy(&mValue, &tmp, sizeof(T));
                break;
            }
            case 4: {
                uint32_t tmp;
                std::memcpy(&tmp, input, sizeof(T));
                tmp = be32toh(tmp);
                std::memcpy(&mValue, &tmp, sizeof(T));
                break;
            }
            case 8: {
                uint64_t tmp;
                std::memcpy(&tmp, input, sizeof(T));
                tmp = be64toh(tmp);
                std::memcpy(&mValue, &tmp, sizeof(T));
                break;
            }
            default:
                throw std::invalid_argument("ntoh invalid type");
        }
    }

    // TODO !pr remove
    // template<>
    // void APIParameter<uint8_t>::customHton(const uint8_t input) {
    //     mBytesRepresentation[1] = input;
    // }
    //
    //
    // template<>
    // void APIParameter<float>::customHton(const float* input) {
    //     constexpr uint8_t BITS_IN_BYTE = 8;
    //
    //     uint32_t input2;
    //     std::memcpy(&input2, input, sizeof(*input));
    //
    //     uint8_t offset = BITS_IN_BYTE * (sizeof(*input) - 1);
    //     for (uint8_t i = 1; i <= sizeof(*input); i++) {
    //         mBytesRepresentation[i] = input2 >> offset;
    //         offset -= BITS_IN_BYTE;
    //     }
    // }
    //
    // template<>
    // void APIParameter<double>::customHton(const double* input) {
    //     constexpr uint8_t BITS_IN_BYTE = 8;
    //
    //     uint64_t input2;
    //     std::memcpy(&input2, input, sizeof(*input));
    //
    //     uint8_t offset = BITS_IN_BYTE * (sizeof(*input) - 1);
    //     for (uint8_t i = 1; i <= sizeof(*input); i++) {
    //         mBytesRepresentation[i] = input2 >> offset;
    //         offset -= BITS_IN_BYTE;
    //     }
    // }
    //
    // template<>
    // void APIParameter<>::customNtoh(const uint8_t* input) {
    //     constexpr uint8_t BITS_IN_BYTE = 8;
    //
    //     uint8_t* data = (uint8_t*)&mValue;
    //     uint8_t offset = BITS_IN_BYTE * (sizeof(T) - 1);
    //     for (uint8_t i = 1; i < sizeof(T); i++) {
    //         data[i] = *input >> offset;
    //         offset -= BITS_IN_BYTE;
    //     }
    // }
    //
    // template<>
    // void APIParameter<float>::customNtoh(const uint8_t* input) {
    //     constexpr uint8_t BITS_IN_BYTE = 8;
    //
    //     uint32_t tmp;
    //     uint8_t* data = (uint8_t*)&tmp;
    //     uint8_t offset = BITS_IN_BYTE * (sizeof(mValue) - 1);
    //     for (uint8_t i = 1; i < sizeof(mValue); i++) {
    //         data[i] = *input >> offset;
    //         offset -= BITS_IN_BYTE;
    //     }
    //
    //     std::memcpy(&mValue, &tmp, sizeof(mValue));
    // }
    // TODO !pr remove



    template<typename T>
    APIParameter<T *>::APIParameter(const T *parameter, const size_t length) {
        mBytesRepresentation.reserve(length + 1);
        mBytesRepresentation.push_back(SpecialByteParameter(parameter, length).getSpecialByte());

        mBytesRepresentation.insert(mBytesRepresentation.end(), parameter, parameter + length);
    }

    template<typename T>
    APIParameter<T *>::APIParameter(const uint8_t *parameter) {
        const SpecialByteParameter sbp(parameter[0]);
        mBytesRepresentation.insert(mBytesRepresentation.begin(), parameter, parameter + sbp.getLength()+1);
    }

    template<typename T>
    SpecialByteParameter APIParameter<T *>::getSpecialByte() const {
        return SpecialByteParameter(mBytesRepresentation[0]);
    }

    template<typename T>
    void APIParameter<T *>::getValue(T* outputArray) {
        std::copy(mBytesRepresentation.begin() + 1, mBytesRepresentation.end(), outputArray);
    }

    template<typename T>
    uint8_t APIParameter<T *>::getBytesRepresentationLength() const {
        return mBytesRepresentation.size();
    }

    template<typename T>
    uint8_t APIParameter<T *>::getValueLength() const {
        return mBytesRepresentation.size() - 1;
    }

    template<typename T>
    void APIParameter<T *>::getBytesRepresentation(uint8_t* outputArray) {
        std::copy(mBytesRepresentation.begin(), mBytesRepresentation.end(), outputArray);
    }
    //
    // template<typename T>
    // void APIParameter<T *>::calculateSpecialByte(T parameter) {
    //
    // }
    //
    // template<typename T>
    // void APIParameter<T *>::getValue() {

    // }





    // template<typename T>
    // void APIParameter<T>::calculateSpecialByte() {
    //     uint8_t type;
    //
    //
    //     // const SpecialByteParameter mSpecialByte(type, sizeof(T));
    //     // mBytesRepresentation[0] = mSpecialByte.getSpecialByte();
    // }

}
