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

    template<typename T>
    APIParameter<T *>::APIParameter(const T *parameter, const size_t length) {
        if (length > 15) throw std::length_error("length too big");

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
}
