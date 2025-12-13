#pragma once

#include <variant>
#include <vector>

#include "api_types.h"
#include "api_parameter.h"
#include "special_byte/special_byte_command.h"
#include "special_byte/special_byte_parameter.h"

namespace Comms::API {
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

    class CommandHandler {
    public:
        explicit CommandHandler(const uint8_t *message);

        explicit CommandHandler(commandTypes commandType);

        commandTypes getCommandType() const;
        String getCommandTypeString() const;
        size_t getNumberOfParameters() const;

        void addParameter(APIParameterVariant APIParameter);
        void generateMessage(uint8_t *messageOutput);

        APIParameterVariant getParameter(uint8_t index) const;

        template<typename T>
        T getParameterValue(uint8_t index) ;

    private:
        void calculateSpecialByteCommand();

        APIParameterVariant createAPIParameterVariant(SpecialByteParameter specialByteParameter, const uint8_t* parameter) const;

        SpecialByteCommand mSpecialByteCommand;


        std::vector<SpecialByteParameter> mParametersSpecialBytes;
        std::vector<APIParameterVariant> mCommandParameters;

        static constexpr uint8_t ms_SPECIAL_BYTE_INDEX = 0;
    };
}
