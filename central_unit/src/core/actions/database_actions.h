#pragma once
#include "actions.h"


namespace SmartHome {
    class DatabaseActions {
        using cmdMetaPtr = std::shared_ptr<Actions::CommandMetadata>;

    public:
        static awaitOptApiResponse databaseRequestHandler(const cmdMetaPtr &commandMetadata);

        static ba::awaitable<API::ApiResponse> sendRequestToDbService(API::ApiRequest &&request,
                                                                      cmdMetaPtr commandMetadata);

    private:
        static bool areParamsValid(API::ApiError &error, const API::InternalApi::Command &command);

        static nlohmann::json &prepareRequestToDatabase(API::ApiRequest &request,
                                                        const API::InternalApi::Command &command);
    };
}
