#include "rf_api.h"
#include "mediator.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace sj = SmartHome::JsonRpcStrings;

namespace SmartHomeMediator {
    RfApi::Parameter::Parameter(uint64_t newValue) {
        size_t length;
        type = ParameterTypes::UINT;

        if (newValue <= std::numeric_limits<uint8_t>::max()) {
            length = sizeof(uint8_t);
        } else if (newValue <= std::numeric_limits<uint16_t>::max()) {
            length = sizeof(uint16_t);
        } else if (newValue <= std::numeric_limits<uint32_t>::max()) {
            length = sizeof(uint32_t);
        } else {
            length = sizeof(uint64_t);
        }

        memcpy(value.data(), &newValue, length); // TODO !pr check
    }

    RfApi::Parameter::Parameter(int64_t newValue) {
        size_t length;
        type = ParameterTypes::INT;

        if (newValue <= std::numeric_limits<int8_t>::max()) {
            length = sizeof(int8_t);
        } else if (newValue <= std::numeric_limits<int16_t>::max()) {
            length = sizeof(int16_t);
        } else if (newValue <= std::numeric_limits<int32_t>::max()) {
            length = sizeof(int32_t);
        } else {
            length = sizeof(int64_t);
        }

        memcpy(value.data(), &newValue, length); // TODO !pr check
    }

    RfApi::Parameter::Parameter(double newValue) {
        size_t length;
        type = ParameterTypes::FLOAT;

        if (newValue <= std::numeric_limits<float>::max()) {
            length = sizeof(float);
        } else {
            length = sizeof(double);
        }

        memcpy(value.data(), &newValue, length); // TODO !pr check
    }

    RfApi::Parameter::Parameter(std::string newValue) {
        auto length = newValue.length();
        type = ParameterTypes::ASCII;

        memcpy(value.data(), &newValue, length); // TODO !pr check
    }

    RfApi::Parameter::Parameter(std::vector<uint8_t> newValue) {
        type = ParameterTypes::RAW;
        value.insert(value.end(), newValue.begin(), newValue.end());
    }

    RfApi::Parameter::Parameter(RfErrorCodes newValue) {
        type = ParameterTypes::ERROR;
        memcpy(value.data(), &newValue, sizeof(newValue)); // TODO !pr check
    }

    RfApi::RfCommand RfApi::toRfCommand(SmartHome::API::ApiRequest apiRequest) {
        RfCommand rfCommand;

        if (!apiRequest.params.has_value()) {
            //TODO !pr error
        }
        const auto &params = apiRequest.params.value();
        if (!(params.contains(sj::ParamsKeys::TARGET) && params.at(sj::ParamsKeys::TARGET) == msMEDIATOR_STRING)) {
            //TODO !pr error
        }

        std::unique_ptr<nlohmann::json> methodParams;
        if (params.contains(sj::ParamsKeys::METHOD_PARAMS)) {
            methodParams = make_unique<nlohmann::json>(params[sj::ParamsKeys::METHOD_PARAMS]);
        }

        const auto method = boost::algorithm::to_lower_copy(apiRequest.method);

        if (apiRequest.id.hasValue()) {
            if (method == msSET_STRING && methodParams != nullptr) {
                rfCommand.command = CommandTypes::SET;
                //TODO !pr
            } else if (method == msGET_STRING && methodParams != nullptr) {
                rfCommand.command = CommandTypes::GET;
                //TODO !pr
            } else if (method == msEXECUTE_STRING && methodParams != nullptr) {
                methodParams->contains("action");
                methodParams->contains("duration");
                // TODO !pr errors

                const auto &action = methodParams->at("action");

                if (action == "sleep") rfCommand.command = CommandTypes::SLEEP;
                else if (action == "deep_sleep") rfCommand.command = CommandTypes::DEEP_SLEEP;
                else {
                    //TODO !pr error
                }

                const auto duration = methodParams->at("duration").get<uint64_t>();
                rfCommand.parameters.emplace(rfCommand.parameters.end(), duration);
            } else if (method == msPING_STRING) {
                rfCommand.command = CommandTypes::PING;
            } else {
                //TODO !pr error
            }

            rfCommand.requestId = apiRequest.id.value();
        } else if (apiRequest.id.isUndefined()) {
            if (method == msNOTIFY_STRING) rfCommand.command = CommandTypes::NOTIFY;
            else {
                // TODO !pr error
            }
        }

        return rfCommand;
    }

    std::string RfApi::toApiString(RfCommand rfCommand) {
        std::string result;

        if (rfCommand.command == CommandTypes::RESPONSE) {
            //TODO !pr handle response
        } else if (rfCommand.command == CommandTypes::NOTIFY) {
            //TODO !pr handle notif
        } else {
            //TODO !pr add error
        }


        return result;
    }

    RfApi::RfApi(const std::shared_ptr<RfClient> &pRfClient,
                 const std::shared_ptr<ApiClient> &pApiClient) : mpRfClient(pRfClient),
                                                                 mpApiClient(pApiClient) {
    }

    void RfApi::handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) {
        // TODO !pr input from RF send to API
        sa::ApiRequest request;
        sa::ApiResponse response;

        //TODO ustalic format w jakim będą wychodzić informacje z RfClienta


        std::string resultMessage;
        mpApiClient->send(resultMessage);
    }

    void RfApi::handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) {
        const auto &mediator = Mediator::Instance();
        sa::ApiRequest request;
        const auto messageView = std::string_view(message);


        // TODO !pr przyjmowanie batch requestów (i pojedyńczych), modyfikacja sesji do obsługi batch requestów
        //  (batch requesty będą przsyłały klika rzeczy w jednej sesji np request i sleep)

        try {
            request(messageView);
        } catch (...) {
            mediator.mpLogger->error("[RF_API] Failed to parse incoming message");
            return;
        }

        if (!request.params.has_value() && request.params->contains(sj::ParamsKeys::METHOD_PARAMS)) {
            mediator.mpLogger->error("[RF_API] Request has no params");
            return;
        }

        auto &params = request.params.value()[sj::ParamsKeys::METHOD_PARAMS];
        // params must have at least module info struct and command type
        if (!params.is_array() && params.size() < 2) {
            mediator.mpLogger->error("[RF_API] Request has invalid params");
            return;
        }

        const bool requestHasId = request.id.hasValue();

        Session::Metadata metadata;

        try {
            metadata = {
                .rfChannel = params[0]["module_rf_channel"].get<uint8_t>(),
                .targetLogicAddress = params[0]["module_id"].get<uint8_t>(),
                .command = params[1].get<std::string>(),
            };

            if (requestHasId) {
                metadata.sessionType = Session::Type::REQUEST;
                metadata.requestId = request.id.value();
            } else {
                metadata.sessionType = Session::Type::NOTIFICATION;
            }

            std::string tmpMetaParams;

            for (int i = 2; i < params.size(); i++) {
                tmpMetaParams += params[i].get<std::string>() + " ";
            }

            if (!tmpMetaParams.empty()) metadata.parameters = tmpMetaParams;
        } catch (const std::exception &e) {
            mediator.mpLogger->errorf("[RF_API] Failed to convert API request to Rf session", e.what());
            return;
        }


        mpRfClient->addToQueue(std::move(metadata));
    }
}
