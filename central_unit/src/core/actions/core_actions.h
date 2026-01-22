#pragma once
#include "actions.h"

#include <string_view>
#include <memory>


namespace ba = boost::asio;

namespace SmartHome {
    /**
     * @brief Core system command handlers.
     *
     * @details Implements handlers for commands targeted at the Core system:
     *          - Echo: Simple echo response for connectivity testing
     *          - Get: Retrieves stored system data
     *          - Set: Configures core parameters (e.g., connection types)
     *          - Notify: Handles notification messages
     *
     * @details Manages connection type registration and cleanup of stale connections.
     */
    class CoreActions {
        using cmdMetaPtr = std::shared_ptr<Actions::CommandMetadata>;

    public:
        enum class SetKeys {
            UNDEFINED = 0,
            CONNECTION_TYPE,
        };

        /**
         * @brief Handle ECHO command for testing/debugging.
         *
         * @details Returns received parameters back to caller with "Core Echo response:" prefix.
         *          Used for connection and parameter passing verification.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with echoed parameters.
         */
        static awaitOptApiResponse coreEchoHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Handle GET command for core data retrieval.
         *
         * @details Queries core cached data with optional condition filtering.
         *          Supports structured queries with target and condition parameters.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with requested data or error.
         *
         * @note Expected params format: [query_target<string>, (optional)conditions<object>]
         * @warning Currently uses temporary hardcoded data - TODO implement core cached data object.
         */
        static awaitOptApiResponse coreGetHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Handle SET command for core configuration.
         *
         * @details Sets core configuration values like connection types.
         *          Validates key-value pairs and delegates to specific setters.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with confirmation or error.
         *
         * @note Expected params format: [key<string>, value<string>]
         */
        static awaitOptApiResponse coreSetHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Handle NOTIFY command for core notifications.
         *
         * @details Processes incoming notifications to core system.
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return std::nullopt (notifications don't return responses).
         *
         * @warning Not yet implemented - TODO implement notify logic.
         */
        static awaitOptApiResponse coreNotifyHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Convert SetKeys enum to string representation.
         *
         * @param setKey SetKeys enum value.
         *
         * @return String representation or "undefined" for unknown values.
         */
        static std::string_view setKeyToString(SetKeys setKey);

        /**
         * @brief Convert string to SetKeys enum.
         *
         * @details Case-insensitive string matching.
         *
         * @param setKey String representation of set key.
         *
         * @return SetKeys enum value or SetKeys::UNDEFINED for unknown strings.
         */
        static SetKeys stringToSetKey(std::string_view setKey);

        /**
         * @brief Set connection type for specific connection.
         *
         * @details Registers connection with specified target type, clearing stale connections first.
         *          Updates both connection-to-type and type-to-connections maps.
         *
         * @param pMetadata Command metadata containing request ID for connection lookup.
         * @param connectionTypeString Target type as string (e.g., "module_mediator", "core").
         *
         * @return true if connection type set successfully, false on error.
         *
         * @note Thread-safe via mutex locks.
         */
        static bool setConnectionType(const cmdMetaPtr &pMetadata,
                                      std::string_view connectionTypeString);

        /**
         * @brief Remove stale connection type mappings.
         *
         * @details Compares active connections against cached mappings and removes entries for closed connections.
         *          Maintains consistency between connection maps and active server connections.
         *
         * @note Thread-safe via mutex locks. Called before modifying connection types.
         */
        static void clearStaleConnectionTypes();

        /// Connection type configuration key string
        static constexpr std::string_view msCONNECTION_TYPE_STRING = "connection_type";
    };
}
