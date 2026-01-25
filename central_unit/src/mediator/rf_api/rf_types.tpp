#pragma once

namespace SmartHomeMediator::RfTypes {
    template<typename T>
        requires std::is_integral_v<T>
    void assignSwappedEndian(std::vector<uint8_t> &buffer, T value) {
        const auto valueSize = sizeof(T);
        buffer.resize(valueSize);

        if constexpr (std::endian::native == std::endian::little) {
            auto swapped = std::byteswap(value);
            memcpy(buffer.data(), &swapped, valueSize);
        } else {
            memcpy(buffer.data(), &value, valueSize);
        }
    }


    template<typename T>
        requires std::is_floating_point_v<T>
    void assignSwappedEndian(std::vector<uint8_t> &buffer, T value) {
        const auto valueSize = sizeof(T);
        buffer.resize(sizeof(T));

        if constexpr (std::endian::native == std::endian::little) {
            if constexpr (std::is_same_v<T, float>) {
                auto raw = std::bit_cast<uint32_t>(value);
                raw = std::byteswap(raw);
                const auto swapped = std::bit_cast<float>(raw);
                memcpy(buffer.data(), &swapped, valueSize);
            } else {
                auto raw = std::bit_cast<uint64_t>(value);
                raw = std::byteswap(raw);
                const auto swapped = std::bit_cast<double>(raw);
                memcpy(buffer.data(), &swapped, valueSize);
            }
        } else {
            memcpy(buffer.data(), &value, valueSize);
        }
    }

    template<typename T>
    T getValueFromRawData(const std::vector<uint8_t> &rawValue) {
        const auto valueSize = sizeof(T);
        T value;
        memcpy(&value, &rawValue[0], valueSize);
        return value;
    }

    template<typename T>
    void copyRawDataToParameter(Parameter &param, const std::span<uint8_t> &rawData) {
        if (rawData.size() > sizeof(T)) throw std::runtime_error("rawData too long for type");

        std::array<uint8_t, sizeof(T)> buffer{};
        std::copy_n(rawData.begin(), rawData.size(), buffer.end() - rawData.size());
        T value = std::bit_cast<T>(buffer);

        param = Parameter(value);
    }

    template<typename T>
    void assignRawDataToParameter(Parameter &param, const std::span<uint8_t> &rawData) {
        T value{};
        value.assign(rawData.begin(), rawData.end());
        param = Parameter(value);
    }

}
