#pragma once
#include "actions.h"


namespace SmartHome {
    class DatabaseActions {
        using cmdMetaPtr = std::shared_ptr<Actions::CommandMetadata>;

    public:
        static awaitOptApiResponse databaseRequestHandler(const cmdMetaPtr &commandMetadata);

        static ba::awaitable<nlohmann::json> getModuleAddressingInfo(uint moduleId);

        static void postSensorReading(uint moduleId, uint sensorLogicId, nlohmann::json value, nlohmann::json metadata);

        static void postLog(uint moduleId, std::string type, std::string content);

        static void updateModuleLastOnline(uint moduleId);
    private:

        static void sendNotificationToDbService(API::ApiRequest &&notification);

        static ba::awaitable<API::ApiResponse> sendRequestToDbService(API::ApiRequest &&request,
                                                                      cmdMetaPtr commandMetadata);

        static bool areParamsValid(API::ApiError &error, const API::InternalApi::Command &command);

        static nlohmann::json &prepareRequestToDatabase(API::ApiRequest &request,
                                                        const API::InternalApi::Command &command);

    };
}
