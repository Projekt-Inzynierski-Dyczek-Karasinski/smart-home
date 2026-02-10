#pragma once
#include "actions.h"

#include <string_view>
#include <memory>
#include <expected>


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
        using getTypeHandler = std::function<awaitOptApiResponse(const cmdMetaPtr &, const nlohmann::json &params)>;

    public:
        /**
         * @brief Set keys supported by core SET handler.
         */
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
         * @details Parses request params and dispatches to specific GET handlers
         *          (modules, sensors, readings, logs, or debug cache).
         *
         * @param commandMetadata Command execution metadata.
         *
         * @return API response with requested data or error.
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
         * @note Expected params format: {key: value}
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
         * @brief Find active connections by connection type string.
         *
         * @details Uses registered connection type mappings to resolve a set of
         *          connection ids for a given type (case-insensitive).
         *
         * @param connectionTypeString Connection type string (e.g., "module_mediator").
         *
         * @return Set of connection ids if found, std::nullopt otherwise.
         */
        static std::optional<std::unordered_set<connectionId_t> > findConnections(
            std::string_view connectionTypeString);

    private:
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

        // Handler helpers
        template<typename T>
        using ValidationResult = std::expected<T, API::ApiError>;

        /**
         * @brief Require module_id in params.
         *
         * @param params Request params JSON.
         *
         * @return module id on success, API error on validation failure.
         */
        static ValidationResult<uint> requireModuleId(const nlohmann::json &params);

        /**
         * @brief Require limit argument in params.
         *
         * @param params Request params JSON.
         *
         * @return limit value on success, API error on validation failure.
         */
        static ValidationResult<uint> requireLimitArg(const nlohmann::json &params);

        /**
         * @brief Require sensor logic id argument in params.
         *
         * @param params Request params JSON.
         *
         * @return sensor logic id on success, API error on validation failure.
         */
        static ValidationResult<uint> requireSensorLogicIdArg(const nlohmann::json &params);

        /**
         * @brief Resolve sensor id from params.
         *
         * @details Resolves from either sensor_id or module_id + sensor_logic_id.
         *
         * @param params Request params JSON.
         *
         * @return resolved sensor id on success, API error on failure.
         */
        static ValidationResult<uint> resolveSensorId(const nlohmann::json &params);

        /**
         * @brief Execute database query for core GET handlers.
         *
         * @param dbQuery JSON query payload for db-service.
         *
         * @return JSON response on success, API error on failure.
         */
        static ba::awaitable<ValidationResult<nlohmann::json> >
        queryDatabase(const nlohmann::json &dbQuery);

        // Core type handlers

        /**
         * @brief Fetch list of modules from database.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON.
         *
         * @return API response with modules list or error.
         */
        static awaitOptApiResponse getModules(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch a single module by id.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON (expects module_id).
         *
         * @return API response with module data or error.
         */
        static awaitOptApiResponse getModule(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch sensors for a module.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON (expects module_id).
         *
         * @return API response with module sensors or error.
         */
        static awaitOptApiResponse getModuleSensors(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch all sensors.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON.
         *
         * @return API response with sensors list or error.
         */
        static awaitOptApiResponse getSensors(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch a single sensor by id.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON (expects sensor_id or module_id + sensor_logic_id).
         *
         * @return API response with sensor data or error.
         */
        static awaitOptApiResponse getSensor(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch readings for a sensor.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON (expects sensor_id or module_id + sensor_logic_id).
         *
         * @return API response with readings list or error.
         */
        static awaitOptApiResponse getSensorReadings(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch log entries for a module.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON (expects module_id, limit).
         *
         * @return API response with logs or error.
         */
        static awaitOptApiResponse getLogs(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch debug view of caches.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON.
         *
         * @return API response with cache dump or error.
         */
        static awaitOptApiResponse getDebugCache(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        // Mediator type handlers

        /**
         * @brief Fetch a reading value from mediator or cache.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON.
         *
         * @return API response with current reading value or error.
         */
        static awaitOptApiResponse getReadingValue(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch module info from mediator.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON.
         *
         * @return API response with module info or error.
         */
        static awaitOptApiResponse getModuleInfo(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Fetch module logs from mediator.
         *
         * @param cmdMetadata Command execution metadata.
         * @param params Request params JSON.
         *
         * @return API response with module logs or error.
         */
        static awaitOptApiResponse getModuleLogs(const cmdMetaPtr &cmdMetadata, const nlohmann::json &params);

        /**
         * @brief Registry of GET type handlers.
         */
        static std::unordered_map<std::string_view, getTypeHandler> msGetTypeHandlersRegistry;
    };
}
