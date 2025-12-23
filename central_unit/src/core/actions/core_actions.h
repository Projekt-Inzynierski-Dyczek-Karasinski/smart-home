#pragma once
#include "actions.h"

#include <string_view>
#include <memory>

#include <boost/asio.hpp>


namespace ba = boost::asio;

namespace SmartHome {
    namespace API {
        struct ApiResponse;
    }

    class CoreActions {
    public:
        enum class SetKeys {
            UNDEFINED = 0,
            CONNECTION_TYPE,
        };

        /**
       * @brief Echo handler for core testing.
       *
       * @details Returns command parameters in response message.
       *
       * @param commandMetadata Command execution metadata.
       * @return API response with echoed parameters.
       */
        static ba::awaitable<API::ApiResponse> coreEchoHandler(
            const std::shared_ptr<Actions::CommandMetadata> &commandMetadata);

        // TODO change docstring after adding proper implementation
        /**
         * @brief Temporary implementation for API testing purposes.
         *
         * @param commandMetadata Command execution metadata.
         * @return API response with echoed parameters.
         *
         * @note Proper implementation will be added later.
         */
        static ba::awaitable<API::ApiResponse> coreGetHandler(
            const std::shared_ptr<Actions::CommandMetadata> &commandMetadata);

        static ba::awaitable<API::ApiResponse> coreSetHandler(
            const std::shared_ptr<Actions::CommandMetadata> &commandMetadata);

        static constexpr std::string_view msCONNECTION_TYPE_STRING = "connection_type";

        static std::string_view setKeyToString(SetKeys setKey);

        static SetKeys stringToSetKey(std::string_view setKey);

        static bool setConnectionType(const std::shared_ptr<Actions::CommandMetadata> &pMetadata,
                                      std::string_view connectionTypeString);

        static void clearStaleConnectionTypes();
    };
}
