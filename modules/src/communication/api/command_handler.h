#pragma once

#include <Arduino.h>
#include <vector>

#include "api_types.h"
#include "api_parameter.h"
#include "special_byte/special_byte_command.h"
#include "special_byte/special_byte_parameter.h"

namespace Comms::API {
    /**
     * @class CommandHandler
     * @brief Utility class to parse, store, and build API command messages.
     * @details The message format is based on a leading "special byte" describing the command and number of arguments,
     * followed by parameter blocks that each start with a parameter "special byte" describing type/length.
     */
    class CommandHandler {
    public:
        /**
         * @brief Construct a CommandHandler by parsing a raw message buffer.
         * @details Expects the first byte of the message to be a command special byte, followed by
         * parameter entries. The number of parameters is derived from the command special byte.
         *
         * @param message Pointer to the received message buffer.
         */
        explicit CommandHandler(const uint8_t *message);

        /**
         * @brief Construct a CommandHandler for creating a new message.
         * Initializes the command special byte with the provided command type.
         *
         * @param commandType Command identifier to use in the generated message.
         */
        explicit CommandHandler(commandTypes commandType);

        /**
         * @brief Get the parsed command type.
         *
         * @return Command type enum value.
         */
        commandTypes getCommandType() const;


        /**
         * @brief Get a human-readable string representation of the command type.
         *
         * @return Arduino String containing the command name.
         */
        String getCommandTypeString() const;

        /**
        * @brief Get the number of parameters currently stored in this command.
        *
        * @return Number of stored parameters.
        */
        size_t getNumberOfParameters() const;

        /**
         * @brief Append a parameter to the command.
         * @details Stores the parameter variant and updates the command special byte so its "argument count"
         * matches the current number of parameters.
         *
         * @param APIParameter Parameter variant to add.
         */
        void addParameter(APIParameterVariant APIParameter);

        /**
         * @brief Serialize the command and its parameters into an output byte buffer.
         *
         * @param messageOutput Destination buffer where the message will be written.
         */
        void generateMessage(uint8_t *messageOutput);

        /**
         * @brief Get a parameter variant by index.
         *
         * @param index Parameter index.
         * @return Copy of the parameter variant stored at index.
         *
         * @throws std::invalid_argument If index is out of range.
         */
        APIParameterVariant getParameter(uint8_t index) const;

        /**
         * @brief Get the typed value of a parameter by index. The method checks the parameter metadata
         * (type/length) stored in its special byte and extracts the value.
         *
         * @tparam T Return type.
         * @param index Parameter index.
         * @return Value converted/extracted as type T.
         *
         * @throws std::invalid_argument If index is out of range or parameter type is unsupported.
         * @throws std::length_error If T is too small for the parameter length (e.g., requesting uint16_t for 4 bytes).
         * @throws std::bad_cast If the stored type/length combination is not valid for extraction.
         */
        template<typename T>
        T getParameterValue(uint8_t index);

    private:
        /**
         * @brief Recompute the command special byte based on the current command type and parameter count.
         */
        void calculateSpecialByteCommand();

        /**
         * @brief Create an APIParameterVariant instance from a parameter special byte and a pointer to its raw bytes.
         *
         * @param specialByteParameter Parsed parameter special byte (encodes type and length).
         * @param parameter Pointer to the beginning of the parameter block in the message buffer.
         * @return Parameter stored as APIParameterVariant.
         *
         * @throws std::invalid_argument If the type/length combination is invalid or unsupported.
         */
        APIParameterVariant createAPIParameterVariant(
            SpecialByteParameter specialByteParameter,
            const uint8_t *parameter
        ) const;

        SpecialByteCommand mSpecialByteCommand;

        std::vector<SpecialByteParameter> mParametersSpecialBytes;
        std::vector<APIParameterVariant> mCommandParameters;

        static constexpr uint8_t ms_SPECIAL_BYTE_INDEX = 0;
    };
}
