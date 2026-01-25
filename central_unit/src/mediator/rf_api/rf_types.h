#pragma once
#include "api.h"

#include <vector>
#include <optional>
#include <variant>

#include <nlohmann/json.hpp>


namespace SmartHomeMediator::RfTypes {

    /**
     * @brief Error codes returned in RF protocol error responses.
     */
    enum class RfErrorCode: uint8_t {
        UNKNOWN = 0,
        BAD_COMMAND,
        UNKNOWN_COMMAND,
        BAD_ARGUMENT,
        NOT_IMPLEMENTED,
        INTERNAL_ERROR
    };

    /**
     * @brief Parameter value types for RF commands.
     */
    enum class ParameterType: uint8_t {
        UNDEFINED = 0,
        // Regular types
        UINT,
        INT,
        FLOAT,
        ASCII,
        // Special types
        RAW,
        ERROR
    };

    /**
     * @brief RF protocol command types.
     */
    enum class RfCommandType: uint8_t {
        UNDEFINED = 0,
        // Low level (Used in rf protocol)
        ACKNOWLEDGE,
        NEGATIVE,
        REPEAT,
        END, // Only sent
        // High level (Handled/executed by target device)
        PING,
        REPING, // Only received
        SLEEP,
        DEEP_SLEEP,
        GET,
        SET,
        RESPONSE, // Only received
        NOTIFY
    };

    /**
     * @brief Types of GET commands for querying module state.
     */
    enum class GetType: uint8_t {
        UNDEFINED = 0,
        // Sensor values
        SENSOR_VALUE,
        // Config values
        CONFIG_OPTION,
        SENSOR_LIST,
        // Debug/Status values
        LOGS,
        BATTERY_LEVEL,
        FORCE_READ_SENSOR_VALUE,
    };

    /**
     * @brief Types of SET commands for controlling modules.
     */
    enum class SetType: uint8_t {
        UNDEFINED = 0,
        // Config values
        CONFIG_OPTION,
        // Actuator values
        TOGGLE_ACTUATOR,
        SET_ACTUATOR_VALUE
    };

    /**
     * @brief Types of notification messages.
     */
    enum class NotificationType: uint8_t {
        UNDEFINED = 0,
        // Received from modules
        MANUAL_TRIGGER,
        POWER_LOSS,
        ALERT,
        // Send to modules
        WAKE
    };

    /**
     * @brief Session initiator and flow type.
     */
    enum class SessionType : uint8_t {
        FROM_CENTRAL_UNIT,
        FROM_MODULE,
        MEDIATOR_CONFIG
    };

    /**
     * @brief Mediator-specific configuration command types.
     */
    enum class MediatorConfigCommandType : uint8_t {
        UNDEFINED = 0,
        SET,
        GET,
        EXECUTE
    };

    /**
     * @brief Discriminator for Command polymorphic type.
     */
    enum class CommandType : uint8_t {
        RF = 0,
        CONFIG
    };

    /**
     * @brief RF protocol packet structure (16 bytes fixed).
     *
     * @details Binary packet format transmitted over RF channel:
     *          - 6 bytes: MAC address (unique network identifier)
     *          - 1 byte: logic address (target module address)
     *          - 1 byte: packets left counter
     *          - 6 bytes: payload (command/data)
     *          - 1 byte: checksum (sum of all bytes modulo 256)
     *          - 1 byte: end marker (always 0x00)
     *
     *          Uses __attribute__((packed)) to disable padding,
     *          ensuring exact 16-byte size for direct memory-to-RF transmission.
     *          Checksum validates data integrity after RF transmission.
     */
    struct Packet {
    private:
        static constexpr uint8_t msPAYLOAD_MAX_SIZE = 6;
        static constexpr uint16_t msCHECKSUM_MODULO = 256;
        static constexpr uint8_t msEND_MARKER = 0;
        static constexpr uint8_t msFILL_SYMBOL = 0;
        static constexpr uint8_t msPACKET_SIZE = 16;

    public:
        std::array<uint8_t, 6> macAddress{};
        uint8_t logicAddress{};
        uint8_t packetsLeft{};
        std::array<uint8_t, msPAYLOAD_MAX_SIZE> payload{};
        uint8_t checksum{};
        uint8_t endMarker{};

        /**
         * @brief Deserialize packet from byte span.
         *
         * @param data Span of at least 16 bytes.
         *
         * @return Packet structure.
         *
         * @throws std::runtime_error if data size < 16 bytes.
         */
        static Packet from_bytes(std::span<const uint8_t> data);

        /**
         * @brief Deserialize packet from byte vector.
         *
         * @param data Vector of at least 16 bytes.
         *
         * @return Packet structure.
         *
         * @throws std::runtime_error if data size < 16 bytes.
         */
        static Packet from_vector(const std::vector<uint8_t> &data);

        /**
         * @brief Get packet as read-only byte span.
         *
         * @return Span view of packet's 16 bytes.
         */
        std::span<const uint8_t> as_bytes() const;

        /**
         * @brief Serialize packet to byte vector.
         *
         * @return Vector with 16-byte packet representation.
         */
        std::vector<uint8_t> to_vector() const;

        /**
         * @brief Validate packet checksum and end marker.
         *
         * @return true if packet passes validation, false otherwise.
         */
        bool isValid() const;

        /**
         * @brief Check if this is the last packet in multi-packet message.
         *
         * @return true if packetsLeft == 0, false otherwise.
         */
        bool isLastPacket() const;

        /**
         * @brief Get maximum payload size in bytes.
         *
         * @return Payload capacity (6 bytes).
         */
        static uint8_t getPayloadMaxSize();

        /**
         * @brief Get end marker byte value.
         *
         * @return End marker constant (0x00).
         */
        static uint8_t getEndMarker();

        /**
         * @brief Get fill symbol for padding payload.
         *
         * @return Fill symbol constant (0x00).
         */
        static uint8_t getFillSymbol();

        /**
         * @brief Get total packet size in bytes.
         *
         * @return Packet size constant (16 bytes).
         */
        static uint8_t getPacketSize();

        /**
         * @brief Calculate and set checksum field.
         *
         * @details Computes checksum as sum of all fields modulo 256.
         */
        void insertChecksum();

        /**
         * @brief Set end marker field to correct value.
         */
        void insertEndMarker();

    private:
        /**
         * @brief Calculate checksum for packet.
         *
         * @param packet Packet to compute checksum for.
         *
         * @return Checksum value (sum modulo 256).
         */
        uint16_t static calculateChecksum(const Packet &packet);

        /**
         * @brief Verify packet checksum matches computed value.
         *
         * @param packet Packet to verify.
         *
         * @return true if checksum valid, false otherwise.
         */
        bool static verifyChecksum(const Packet &packet);
    } __attribute__((packed));

    /**
     * @brief Typed parameter container for RF commands.
     *
     * @details Stores value with type tag for serialization/deserialization.
     *          Supports numeric types, strings, raw bytes, and error codes.
     */
    struct Parameter {
        ParameterType type{};
        std::vector<uint8_t> value{};

        Parameter() = default;

        /**
         * @brief Construct UINT parameter.
         *
         * @param newValue Unsigned integer value (up to 64-bit).
         */
        explicit Parameter(uint64_t newValue);

        /**
         * @brief Construct INT parameter.
         *
         * @param newValue Signed integer value (up to 64-bit).
         */
        explicit Parameter(int64_t newValue);

        /**
         * @brief Construct FLOAT parameter.
         *
         * @param newValue Floating point value (double precision).
         */
        explicit Parameter(double newValue);

        /**
         * @brief Construct ASCII parameter.
         *
         * @param newValue String value.
         */
        explicit Parameter(std::string_view newValue);

        /**
         * @brief Construct RAW parameter.
         *
         * @param newValue Raw byte data.
         */
        explicit Parameter(const std::vector<uint8_t> &newValue);

        /**
         * @brief Construct ERROR parameter.
         *
         * @param newValue RF error code.
         */
        explicit Parameter(RfErrorCode newValue);

        /**
         * @brief Deserialize parameter from JSON object.
         *
         * @param json JSON object with singular value.
         *
         * @return Parameter with parsed type and value.
         *
         * @throws std::exception on invalid JSON structure or type mismatch.
         */
        static Parameter parameterFromJson(const nlohmann::json &json);

        /**
         * @brief Serialize parameter to JSON object.
         *
         * @return JSON object with singular value.
         */
        nlohmann::json parameterToJson() const;

        /**
         * @brief Serialize parameter to binary format.
         *
         * @details Format: [special byte][value bytes with endianness conversion].
         *
         * @return Vector with binary representation.
         */
        std::vector<uint8_t> to_vector() const;
    };


    class Command;

    /**
     * @brief Session configuration and command queue.
     *
     * @details Contains target addressing, channel selection, and vector of commands to execute in session.
     */
    struct SessionMetadata {
        SessionType sessionType = SessionType::FROM_CENTRAL_UNIT;
        uint8_t rfChannel = 0;
        // Logic address for module must be more than 0, default value of 0 is reserved for mediator
        uint8_t targetLogicAddress = 0;
        std::vector<std::unique_ptr<Command> > commands{};

        // Delete copy
        SessionMetadata(const SessionMetadata &) = delete;

        SessionMetadata &operator=(const SessionMetadata &) = delete;

        // Allow move
        SessionMetadata(SessionMetadata &&) = default;

        SessionMetadata &operator=(SessionMetadata &&) = default;

        SessionMetadata() = default;
    };

    /**
     * @brief Base class for polymorphic command types.
     */
    class Command {
    protected:
        const CommandType mType;

    public:
        explicit Command(const CommandType type) : mType(type) {
        }

        virtual ~Command() = default;

        /**
         * @brief Command type getter.
         *
         * @return Command type.
         */
        CommandType getType() const;

        std::optional<SmartHome::apiId_t> requestId;
    };

    /**
     * @brief RF protocol command with typed parameters.
     */
    class RfCommand : public Command {
    public:
        RfCommand() : Command(CommandType::RF) {
        }

        /**
         * @brief Deserialize RF command from binary data.
         *
         * @param rawData Binary command bytes.
         *
         * @throws std::exception on parsing errors.
         */
        explicit RfCommand(std::vector<uint8_t> rawData);

        RfCommandType rfCommandType = RfCommandType::UNDEFINED;

        std::optional<std::variant<GetType, SetType, NotificationType> > requestType;

        std::vector<Parameter> parameters;

        /**
         * @brief Serialize RF command to binary format.
         *
         * @details Format: [command type][optional request type][parameters].
         *
         * @return Vector with binary representation for RF transmission.
         */
        std::vector<uint8_t> to_vector() const;
    };

    /**
     * @brief Mediator-internal configuration command.
     */
    class MediatorConfigCommand : public Command {
    public:
        MediatorConfigCommand() : Command(CommandType::CONFIG) {
        }

        MediatorConfigCommandType configCommandType = MediatorConfigCommandType::UNDEFINED;

        std::string commandKey;

        std::string commandValue;
    };


    /**
     * @brief Convert RF error code to string representation.
     *
     * @param code RF error code enum value.
     *
     * @return String name of error code (e.g., "BAD_COMMAND").
     */
    std::string_view getStringFromRfErrorCode(RfErrorCode code);

    /**
     * @brief Convert string to GetType enum.
     *
     * @param value String representation (e.g., "sensor_value").
     *
     * @return GetType enum value, or GetType::UNDEFINED if unknown.
     */
    GetType getTypeFromString(std::string_view value);

    /**
     * @brief Convert string to SetType enum.
     *
     * @param value String representation (e.g., "toggle_actuator").
     *
     * @return SetType enum value, or SetType::UNDEFINED if unknown.
     */
    SetType setTypeFromString(std::string_view value);

    /**
     * @brief Convert string to NotificationType enum.
     *
     * @param value String representation (e.g., "manual_trigger").
     *
     * @return NotificationType enum value, or NotificationType::UNDEFINED if unknown.
     */
    NotificationType notificationTypeFromString(std::string_view value);

    /**
     * @brief Convert NotificationType enum to string.
     *
     * @param value NotificationType enum value.
     *
     * @return String representation, or "undefined" if unknown.
     */
    std::string_view notificationTypeToString(NotificationType value);

    /**
     * @brief Pack two 4-bit values into single byte.
     *
     * @param firstHalf Upper 4 bits (0-15).
     * @param secondHalf Lower 4 bits (0-15).
     *
     * @return Byte with firstHalf in upper nibble, secondHalf in lower nibble.
     */
    uint8_t getSpecialByte(uint8_t firstHalf, uint8_t secondHalf);

    /**
     * @brief Unpack byte into two 4-bit values.
     *
     * @param specialByte Packed byte.
     *
     * @return Pair of (upper nibble, lower nibble) values.
     */
    std::pair<uint8_t, uint8_t> readSpecialByte(uint8_t specialByte);


    /**
     * @brief Serialize integral value to buffer with endianness conversion.
     *
     * @tparam T Integral type (uint8_t, uint16_t, uint32_t, uint64_t, int variants).
     * @param buffer Vector to resize and fill (existing content is discarded).
     * @param value Value to serialize.
     *
     * @details Resizes buffer to sizeof(T).
     *          On little-endian systems, swaps bytes to big-endian (network byte order) before copying.
     *          On big-endian systems, copies value directly.
     */
    template<typename T>
        requires std::is_integral_v<T>
    void assignSwappedEndian(std::vector<uint8_t> &buffer, T value);

    /**
     * @brief Serialize floating-point value to buffer with endianness conversion.
     *
     * @tparam T Floating-point type (float, double).
     * @param buffer Vector to resize and fill (existing content is discarded).
     * @param value Value to serialize.
     *
     * @details Resizes buffer to sizeof(T).
     *          On little-endian systems, bit-casts float/double to uint32_t/uint64_t, swaps bytes, bit-casts back,
     *          then copies.
     *          On big-endian systems, copies value directly.
     */
    template<typename T>
        requires std::is_floating_point_v<T>
    void assignSwappedEndian(std::vector<uint8_t> &buffer, T value);

    /**
    * @brief Deserialize value from raw byte data.
    *
    * @tparam T Target type.
    * @param rawValue Raw bytes.
    *
    * @return Deserialized value copied from rawValue.
    */
    template<typename T>
    T getValueFromRawData(const std::vector<uint8_t> &rawValue);

    /**
    * @brief Create parameter from right-aligned raw data.
    *
    * @tparam T Numeric type for parameter (determines parameter type).
    * @param param Parameter to update with new value.
    * @param rawData Source bytes (must not exceed sizeof(T) bytes).
    *
    * @details Creates sizeof(T), copies rawData to the END of buffer (right-aligned), bit-casts to type T,
    *          then creates Parameter.
    *
    * @throws std::runtime_error if rawData.size() > sizeof(T).
    */
    template<typename T>
    void copyRawDataToParameter(Parameter &param, const std::span<uint8_t> &rawData);

    /**
     * @brief Create parameter from raw data using container assignment.
     *
     * @tparam T Container type with assign() method (e.g., std::vector, std::string).
     * @param param Parameter to update with new value.
     * @param rawData Source bytes to copy into container.
     *
     * @details Creates container of type T, calls assign() to copy rawData contents,
     *          then creates Parameter from container.
     */
    template<typename T>
    void assignRawDataToParameter(Parameter &param, const std::span<uint8_t> &rawData);


    // Target string
    inline constexpr std::string_view MEDIATOR_STRING = "module_mediator";
    inline constexpr std::string_view CORE_STRING = "core";

    // Commands strings
    inline constexpr std::string_view GET_STRING = "get";
    inline constexpr std::string_view SET_STRING = "set";
    inline constexpr std::string_view NOTIFY_STRING = "notify";
    inline constexpr std::string_view EXECUTE_STRING = "execute";
    inline constexpr std::string_view PING_STRING = "ping";

    // Set/Get/Notify type strings
    inline constexpr std::string_view CONFIG_OPTION_STRING = "config_option";
    inline constexpr std::string_view TOGGLE_ACTUATOR_STRING = "toggle_actuator";
    inline constexpr std::string_view SET_ACTUATOR_VALUE_STRING = "set_actuator_value";
    inline constexpr std::string_view SENSOR_VALUE_STRING = "sensor_value";
    inline constexpr std::string_view SENSOR_LIST_STRING = "sensor_list";
    inline constexpr std::string_view LOGS_STRING = "logs";
    inline constexpr std::string_view BATTERY_LEVEL_STRING = "battery_level";
    inline constexpr std::string_view FORCE_READ_SENSOR_VALUE_STRING = "force_read_sensor_value";
    inline constexpr std::string_view MANUAL_TRIGGER_STRING = "manual_trigger";
    inline constexpr std::string_view POWER_LOSS_STRING = "power_loss";
    inline constexpr std::string_view ALERT_STRING = "alert";
    inline constexpr std::string_view WAKE_STRING = "wake";

    // Action types strings
    inline constexpr std::string_view SLEEP_STRING = "sleep";
    inline constexpr std::string_view DEEP_SLEEP_STRING = "deep_sleep";

    // Special strings
    inline constexpr std::string_view ALL_OPTIONS_STRING = "all_options";
    inline constexpr std::string_view DEFAULT_STRING = "DEFAULT";
    inline constexpr std::string_view UNDEFINED_STRING = "undefined";
}

#include "rf_types.tpp"
