#pragma once
#include "rf_driver/hc12_driver.h"

#include <string>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <queue>
#include <span>
#include <vector>
#include <utility>

#include <boost/asio.hpp>


namespace ba = boost::asio;

namespace SmartHomeMediator {
    using namespace std::chrono_literals;

    struct Packet {
    private:
        static constexpr uint8_t msPAYLOAD_MAX_SIZE = 6;
        static constexpr uint16_t msCHECKSUM_MODULO = 256;
        static constexpr uint8_t msEND_MARKER = 0;
        static constexpr uint8_t msFILL_SYMBOL = 0;

    public:
        std::array<uint8_t, 6> macAddress;
        uint8_t logicAddress;
        uint8_t packetsLeft;
        std::array<uint8_t, msPAYLOAD_MAX_SIZE> payload;
        uint8_t checksum;
        uint8_t endMarker;

        static Packet from_bytes(std::span<const uint8_t> data);

        static Packet from_vector(const std::vector<uint8_t> &data);

        std::span<const uint8_t> as_bytes() const;

        std::vector<uint8_t> to_vector() const;

        bool isValid() const;

        bool isLastPacket() const;

        // std::vector<uint8_t> getPayload() const;

        static uint8_t getPayloadMaxSize();

        static uint8_t getEndMarker();

        static uint8_t getFillSymbol();

        void insertChecksum();

        void insertEndMarker();

    private:
        uint16_t static calculateChecksum(const Packet &packet);

        bool static verifyChecksum(const Packet &packet);
    } __attribute__((packed));


    class Session {
    public:
        enum class Type : uint8_t {
            REQUEST,
            NOTIFICATION,
            NOTIFICATION_FROM_MODULE
        };

        struct Metadata {
            Type sessionType;
            uint8_t rfChannel;
            uint8_t targetLogicAddress;
            std::string command;
            std::optional<std::string> parameters;
            std::optional<size_t> requestId;
        };

        Session(const Metadata &metadata, const std::shared_ptr<HC12Driver> &rfDriver);

        ~Session();

        ba::awaitable<std::string> execute();

        void addReceivedPacket(Packet packet);

    private:
        enum class State : uint8_t {
            SENDING_REQUEST,
            SENDING_NOTIFICATION,
            SENDING_COMMAND,
            SENDING_NOTIFICATION_ACKNOWLEDGMENT,
            SENDING_REPEAT_LAST_MESSAGE,
            SENDING_END_SESSION,
            AWAITING_RESPONSE,
            AWAITING_ACKNOWLEDGMENT,
            RESENDING_LAST_MESSAGE,
        };


        ba::awaitable<void> send(std::vector<uint8_t> message);

        ba::awaitable<std::vector<uint8_t> > receive();


        ba::awaitable<bool> changeChannel(uint8_t channel) const;

        /**
         * @brief Joins 2 uint numbers (with values less than 16) to one byte value using bit shift.
         *
         * @param value1 Half byte that is shifted to more significant bits.
         * @param value2 Half byte that is joined as less significant half of byte.
         *
         * @return 4 bits of value1 joined with 4 bits of value2.
         * @note Both values must be 4 bit (less than 16).
         */
        static uint8_t joinHalfBytesIntoByte(uint8_t value1, uint8_t value2);


        /**
         * @brief Get special byte for RfCommand with number of fallowing parameters.
         *
         * @param command RfCommand to be sent/executed, saved in more significant half of a byte.
         * @param numberOfParameters Numbers of parameters to be read, saved in less significant half of byte.
         *
         * @return Special byte with first 4 bits representing RfCommand and later 4 bits numbers of params following.
         */
        static uint8_t getSpecialByte(RfCommands command, uint8_t numberOfParameters);


        /**
         * @brief Get special byte for RfCommandParameter with param type and size.
         *
         * @param parameterType Parameter type, saved in more significant half of byte.
         * @param parameterSize Parameter size, saved in less significant half of byte.
         *
         * @return Special byte with first 4 bits representing param type and later 4 bits parameter size in bytes.
         */
        static uint8_t getSpecialByte(RfCommandsParameterTypes parameterType, uint8_t parameterSize);

        static std::pair<uint8_t, uint8_t> parseSpecialByte(uint8_t specialByte);

        static constexpr uint ms_MAX_REATTEMPTS = 3;
        static constexpr auto ms_SESSION_TIMEOUT = 10s;

        const Metadata mMetadata;
        std::shared_ptr<HC12Driver> mpRfDriver;

        State mSessionCurrentState{};

        uint mFailedAttempts = 0;


        // public:
        //     Session(uint8_t logicAddress, std::string_view macAddress, std::string_view message);
        //
        //     explicit Session(const Packet &packet);
        //
        //     std::optional<Packet> getOutgoingPacket();
        //
        //     void addIncomingPacket(const Packet &packet);
        //
        // private:
        //     Packet createNextPacket();
        //
        //     static inline std::array<uint8_t, 6> stringMacToArray(std::string_view macAddress);
        //
        //     // static inline std::string arrayMacToString(const std::array<uint8_t, 6> &macAddress);
        //
        //     const std::array<uint8_t, 6> mMacAddress = stringMacToArray("30:9c:23:a2:02:e8"); //TODO !pr config
        //
        //     const uint8_t mLogicAddress;
        //     std::vector<uint8_t> mMessage;
        //
        //     size_t mMessageOffset = 0;
        //     size_t mPacketsLeft{};
    };
}
