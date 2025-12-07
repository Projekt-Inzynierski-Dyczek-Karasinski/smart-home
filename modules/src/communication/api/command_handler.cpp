#include "command_handler.h"

#include <stdexcept>

namespace Comms::API {
    CommandHandler::CommandHandler(const uint8_t *message)
        : mSpecialByteCommand(SpecialByteCommand(message[ms_SPECIAL_BYTE_INDEX])) {
        uint8_t messageOffset = 1;
        for (uint8_t i = 0; i < mSpecialByteCommand.getNumOfArguments(); i++) {
            SpecialByteParameter sbp(message[messageOffset]);

            mCommandParameters.push_back(
            createAPIParameterVariant(sbp, &message[messageOffset])
            );
            mParametersSpecialBytes.push_back(SpecialByteParameter(message[messageOffset]));

            messageOffset += sbp.getLength() + 1;
        }
    }

    CommandHandler::CommandHandler(const commandTypes commandType)
        : mSpecialByteCommand(commandType, 0) {}


    commandTypes CommandHandler::getCommandType() const {
        return mSpecialByteCommand.getCommandType();
    }
    size_t CommandHandler::getNumberOfParameters() const {
        return mCommandParameters.size();
    }

    void CommandHandler::addParameter(const APIParameterVariant APIParameter) {
        mCommandParameters.push_back(APIParameter);
        calculateSpecialByteCommand();
    }


    void CommandHandler::generateMessage(uint8_t *messageOutput) {
        calculateSpecialByteCommand();
        messageOutput[ms_SPECIAL_BYTE_INDEX] = mSpecialByteCommand.getSpecialByte();
        uint8_t messageOffset = 1;

        for (uint8_t i = 0; i < getNumberOfParameters(); i++) {
            std::visit([&messageOutput, &messageOffset](auto&& param) {
                param.getBytesRepresentation(&messageOutput[messageOffset]);
                messageOffset += param.getBytesRepresentationLength();
            }, mCommandParameters[i]);
        }
    }

    APIParameterVariant CommandHandler::getParameter(const uint8_t index) const {
        return mCommandParameters[index];
    }


    void CommandHandler::calculateSpecialByteCommand() {
        const SpecialByteCommand newSpecialByteCommand(mSpecialByteCommand.getCommandType(), getNumberOfParameters());
        mSpecialByteCommand = newSpecialByteCommand;
    }

    APIParameterVariant CommandHandler::createAPIParameterVariant(const SpecialByteParameter specialByteParameter, const uint8_t* parameter) const {
        using PT = parametersTypes;

        switch (specialByteParameter.getType()) {
            case (uint8_t)PT::UINT:
                switch (specialByteParameter.getLength()) {
                    case 1: return APIParameter<uint8_t>(parameter);
                    case 2: return APIParameter<uint16_t>(parameter);
                    case 4: return APIParameter<uint32_t>(parameter);
                    case 8: return APIParameter<uint64_t>(parameter);
                    default: throw std::invalid_argument("Invalid length for UINT");
                }

            case (uint8_t)PT::INT:
                switch (specialByteParameter.getLength()) {
                    case 1: return APIParameter<int8_t>(parameter);
                    case 2: return APIParameter<int16_t>(parameter);
                    case 4: return APIParameter<int32_t>(parameter);
                    case 8: return APIParameter<int64_t>(parameter);
                    default: throw std::invalid_argument("Invalid length for INT");
                }

            case (uint8_t)PT::FLOAT:
                switch (specialByteParameter.getLength()) {
                    case 4: return APIParameter<float>(parameter);
                    case 8: return APIParameter<double>(parameter);
                    default: throw std::invalid_argument("Invalid length for FLOAT");
                }

            case (uint8_t)PT::ERROR:
                switch (specialByteParameter.getLength()) {
                    case 1: return APIParameter<uint8_t>(parameter);
                    default: throw std::invalid_argument("Invalid length for ERROR");
                }

            case (uint8_t)PT::ASCII:
                return APIParameter<char*>(parameter);
            case (uint8_t)PT::RAW:
                return APIParameter<uint8_t*>(parameter);
            default:
                throw std::invalid_argument("Unsupported parameter type");
        }
    }

    template<typename T>
    T CommandHandler::getParameterValue(const uint8_t index) {
        using PT = parametersTypes;
        switch (mParametersSpecialBytes[index].getType()) {
            case (uint8_t)PT::UINT:
                switch (mParametersSpecialBytes[index].getLength()) {
                    case 1:
                        return std::get<APIParameter<uint8_t>>(mCommandParameters[index]).getValue();
                    case 2:
                        if (sizeof(T) < 2) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<uint16_t>>(mCommandParameters[index]).getValue();
                    case 4:
                        if (sizeof(T) < 4) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<uint32_t>>(mCommandParameters[index]).getValue();
                    case 8:
                        if (sizeof(T) < 8) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<uint64_t>>(mCommandParameters[index]).getValue();
                    default:
                        throw std::bad_cast();
                }

            case (uint8_t)PT::INT:
                switch (mParametersSpecialBytes[index].getLength()) {
                    case 1:
                        return std::get<APIParameter<int8_t>>(mCommandParameters[index]).getValue();
                    case 2:
                        if (sizeof(T) < 2) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<int16_t>>(mCommandParameters[index]).getValue();
                    case 4:
                        if (sizeof(T) < 4) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<int32_t>>(mCommandParameters[index]).getValue();
                    case 8:
                        if (sizeof(T) < 8) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<int64_t>>(mCommandParameters[index]).getValue();
                    default:
                        throw std::bad_cast();
                }

            case (uint8_t)PT::FLOAT:
                switch (mParametersSpecialBytes[index].getLength()) {
                    case 4:
                        if (sizeof(T) < 4) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<float>>(mCommandParameters[index]).getValue();
                    case 8:
                        if (sizeof(T) < 8) throw std::length_error("Variable is too small for APIParameter");
                        return std::get<APIParameter<double>>(mCommandParameters[index]).getValue();
                    default:
                        throw std::bad_cast();
                }

            case (uint8_t)PT::ERROR:
                switch (mParametersSpecialBytes[index].getLength()) {
                    case 1:
                        return std::get<APIParameter<uint8_t>>(mCommandParameters[index]).getValue();
                    default:
                        throw std::invalid_argument("Invalid length for ERROR");
                }

            default:
                throw std::invalid_argument("Unsupported parameter type");
        }
    }

    template uint8_t CommandHandler::getParameterValue<uint8_t>(uint8_t);
    template uint16_t CommandHandler::getParameterValue<uint16_t>(uint8_t);
    template uint32_t CommandHandler::getParameterValue<uint32_t>(uint8_t);
    template uint64_t CommandHandler::getParameterValue<uint64_t>(uint8_t);
    template int8_t CommandHandler::getParameterValue<int8_t>(uint8_t);
    template int16_t CommandHandler::getParameterValue<int16_t>(uint8_t);
    template int32_t CommandHandler::getParameterValue<int32_t>(uint8_t);
    template int64_t CommandHandler::getParameterValue<int64_t>(uint8_t);
    template float CommandHandler::getParameterValue<float>(uint8_t);
    template double CommandHandler::getParameterValue<double>(uint8_t);
}
