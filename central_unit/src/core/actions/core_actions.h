#pragma once
#include "actions.h"

#include <string_view>
#include <memory>
#include <expected>

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
        using coreCommandHandler = std::function<awaitOptApiResponse(cmdMetaPtr, jsonPtr)>;

    public:
        // Method handlers

        /**
         * @brief Handle ECHO command for testing/debugging.
         *
         * @details Returns received parameters back to caller with "Core Echo response:" prefix.
         *          Used for connection and parameter passing verification.
         *
         * @param pCommandMetadata Command execution metadata.
         *
         * @return API response with echoed parameters.
         */
        static awaitOptApiResponse coreEchoHandler(cmdMetaPtr pCommandMetadata);

        /**
         * @brief Handle GET command for core data retrieval.
         *
         * @details Parses request params and dispatches to specific GET handlers
         *          (modules, devices, readings, logs, or debug cache).
         *
         * @param pCommandMetadata Command execution metadata.
         *
         * @return API response with requested data or error.
         */
        static awaitOptApiResponse coreGetHandler(cmdMetaPtr pCommandMetadata);

        /**
         * @brief Handle SET command for core configuration.
         *
         * @details Sets core configuration values like connection types.
         *          Validates key-value pairs and delegates to specific setters.
         *
         * @param pCommandMetadata Command execution metadata.
         *
         * @return API response with confirmation or error.
         *
         * @note Expected params format: {key: value}
         */
        static awaitOptApiResponse coreSetHandler(cmdMetaPtr pCommandMetadata);

        /**
         * @brief Handle DELETE command for core data removal.
         *
         * @details Parses request params, resolves entity type (module or device),
         *          and delegates to \c deleteDatabaseValue.
         *          Supports deleting entire records or specific JSONB field values via path.
         *
         * @param pCommandMetadata Command execution metadata.
         *
         * @return API response with status confirmation or error.
         */
        static awaitOptApiResponse coreDeleteHandler(cmdMetaPtr pCommandMetadata);

        /**
         * @brief Handle NOTIFY command for core notifications.
         *
         * @details Processes incoming notifications to core system.
         *
         * @param pCommandMetadata Command execution metadata.
         *
         * @return std::nullopt (notifications don't return responses).
         *
         * @warning Not yet implemented - TODO implement notify logic.
         */
        static awaitOptApiResponse coreNotifyHandler(cmdMetaPtr pCommandMetadata);


        // Utility

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
        // Core type handlers

        /**
         * @brief Fetch list of modules from database.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON. (unused)
         *
         * @return API response with modules list or error.
         */
        static awaitOptApiResponse getModules(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch a single module by id.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects module_id).
         *
         * @return API response with module data or error.
         */
        static awaitOptApiResponse getModule(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch devices for a module.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects module_id).
         *
         * @return API response with module devices or error.
         */
        static awaitOptApiResponse getModuleDevices(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch all devices.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON. (unused)
         *
         * @return API response with devices list or error.
         */
        static awaitOptApiResponse getDevices(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch a single device by id.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects device_id or module_id + device_logic_id).
         *
         * @return API response with device data or error.
         */
        static awaitOptApiResponse getDevice(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch readings for a device.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects device_id or module_id + device_logic_id).
         *
         * @return API response with readings list or error.
         */
        static awaitOptApiResponse getDeviceReadings(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch log entries for a module.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects module_id, limit).
         *
         * @return API response with logs or error.
         */
        static awaitOptApiResponse getLogs(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch debug view of caches.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON. (unused)
         *
         * @return API response with cache dump or error.
         */
        static awaitOptApiResponse getDebugCache(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Delete a database record or JSONB field value.
         *
         * @details Resolves entity ID from params (module_id or device_id/logic_id).
         *          If \c path param is present, removes the specified JSONB field using the
         *          PostgreSQL \c #- operator. Otherwise deletes the entire entity record.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects type, module_id or device_id, optional path).
         *
         * @return API response with status confirmation or error.
         */
        static awaitOptApiResponse deleteDatabaseValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams);


        // Mediator type handlers

        /**
         * @brief Fetch a reading value from mediator or cache.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON.
         *
         * @return API response with current reading value or error.
         */
        static awaitOptApiResponse getReadingValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch module info from mediator.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON.
         *
         * @return API response with module info or error.
         */
        static awaitOptApiResponse getModuleInfo(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Fetch module logs from mediator.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON.
         *
         * @return API response with module logs or error.
         */
        static awaitOptApiResponse getModuleLogs(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Register connection type for the current connection.
         *
         * @details Reads \c value param as connection type string, clears stale connections,
         *          then registers the current connection ID in the connection type maps.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects value with connection type string).
         *
         * @return API response with status confirmation or error.
         */
        static awaitOptApiResponse setConnectionType(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Create or update a database record value.
         *
         * @details If \c values param is present, inserts a new record.
         *          Otherwise requires \c mode, \c path, and \c value params to update an existing
         *          module or device record via \c updateModule or \c updateDevice.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects type and either values or mode+path+value).
         *
         * @return API response with status confirmation or error.
         */
        static awaitOptApiResponse setDatabaseValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams);

        /**
         * @brief Delegate a SET command directly to a module via mediator.
         *
         * @details Resolves module_id and device_id from params, then forwards the
         *          command with its args to \c MediatorActions::sendToModule.
         *
         * @param pCommandMetadata Command execution metadata.
         * @param pParams Request params JSON (expects module_id, device_id/logic_id, type, args).
         *
         * @return API response with result from mediator or error.
         */
        static awaitOptApiResponse setModuleValue(cmdMetaPtr pCommandMetadata, jsonPtr pParams);


        // Command handler-type registers

        /**
         * @brief Registry of GET type handlers.
         */
        static std::unordered_map<std::string_view, coreCommandHandler> msGetTypeHandlersRegistry;

        /**
         * @brief Registry of SET type handlers.
         */
        static std::unordered_map<std::string_view, coreCommandHandler> msSetTypeHandlersRegistry;


        // Utility

        /**
         * @brief Remove stale connection type mappings.
         *
         * @details Compares active connections against cached mappings and removes entries for closed connections.
         *          Maintains consistency between connection maps and active server connections.
         *
         * @note Thread-safe via mutex locks. Called before modifying connection types.
         */
        static void clearStaleConnectionTypes();


        /**
         * @brief Update a module record in the database.
         *
         * @details Validates module existence in cache, then builds \c EntityUpdateContext
         *          and delegates to \c updateEntity.
         *
         * @param pParams Request params JSON (expects module_id).
         * @param mode Update mode (e.g. overwrite or append).
         * @param path Dot-separated path to the field being updated.
         * @param value New value to set.
         *
         * @return Serialized result string on success, API error on failure.
         */
        static ba::awaitable<std::expected<std::string, API::ApiError> > updateModule(jsonPtr pParams,
            std::string &&mode,
            std::string &&path,
            nlohmann::json &&value);

        /**
         * @brief Update a device record in the database.
         *
         * @details Validates device existence in cache, then builds \c EntityUpdateContext
         *          and delegates to \c updateEntity.
         *
         * @param pParams Request params JSON (expects device_id or module_id + device_logic_id).
         * @param mode Update mode (e.g. overwrite or append).
         * @param path Dot-separated path to the field being updated.
         * @param value New value to set.
         *
         * @return Serialized result string on success, API error on failure.
         */
        static ba::awaitable<std::expected<std::string, API::ApiError> > updateDevice(jsonPtr pParams,
            std::string &&mode,
            std::string &&path,
            nlohmann::json &&value);

        /// Variant holding either a cached module or device entity.
        using CachedEntity = std::variant<CachedModule, CachedDevice>;

        /**
         * @brief Context for generic entity update operations.
         */
        struct EntityUpdateContext {
            /// Request params JSON
            jsonPtr pParams;
            /// Database table name for this entity type
            std::string_view tableName;
            /// Human-readable entity name used in error messages
            std::string_view entityName;
            /// Entity identifier
            uint id;
            /// Returns cached entity, or nullopt if not in cache
            std::function<std::optional<CachedEntity>()> getCache;
            /// Atomically compares and exchanges the freshness flag; returns the previous value
            std::function<std::optional<bool>(bool, bool)> compareExchangeFresh;
            /// Refreshes entity cache from the database
            std::function<ba::awaitable<void>()> refreshCache;
        };

        /**
         * @brief Resolve a JSONB field value from a cached entity.
         *
         * @details Visits the entity variant and returns the field matching \c fieldName.
         *          Currently only supports the \c config field.
         *
         * @param entity Cached module or device entity.
         * @param fieldName Name of the JSONB field to resolve.
         *
         * @return JSON field value if found, std::nullopt otherwise.
         */
        static std::optional<nlohmann::json> resolveJsonbField(CachedEntity &entity, std::string_view fieldName);

        /**
         * @brief Retrieve a fresh entity from cache, refreshing if stale.
         *
         * @details Uses \c compareExchangeFresh to check freshness atomically.
         *          If stale, calls \c refreshCache and verifies freshness again before returning.
         *
         * @param ctx Entity update context with cache access callbacks.
         *
         * @return Cached entity on success, API error if cache is unavailable or refresh fails.
         */
        static ba::awaitable<std::expected<CachedEntity, API::ApiError> > resolveEntityCache(
            const EntityUpdateContext &ctx);

        /**
         * @brief Apply a field update to a database entity.
         *
         * @details Resolves the current entity from cache, then builds a db query
         *          according to \c mode:
         *          - \c append: reads the cached JSONB field, merges/pushes \c value, then overwrites.
         *          - \c overwrite: sets the field at \c path directly in the database.
         *
         * @param ctx Entity update context.
         * @param mode Update mode (\c append or \c overwrite).
         * @param path Dot-separated path to the target field.
         * @param value New value to apply.
         *
         * @return Serialized database response on success, API error on failure.
         */
        static ba::awaitable<std::expected<std::string, API::ApiError> > updateEntity(EntityUpdateContext &&ctx,
            std::string &&mode,
            std::string &&path,
            nlohmann::json &&value);
    };

}
