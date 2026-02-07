#pragma once
#include "actions.h"


namespace SmartHome {
    /**
     * @brief Database related command handlers
     *
     * @details Implements wrappers that translate internal API requests into database API requests sent to db-service
     *          and helpers that post asynchronous notifications.
     */
    class DatabaseActions {
        using cmdMetaPtr = std::shared_ptr<Actions::CommandMetadata>;

    public:
        /**
         * @brief Handle internal database-targeted command.
         *
         * @details Validates parameters, prepares an API request intended for the database
         *          service and forwards the request, awaiting the response to return.
         *
         * @param commandMetadata Metadata describing the incoming internal command.
         *
         * @return API response from db-service or error describing validation/dispatch failure.
         */
        static awaitOptApiResponse databaseRequestHandler(const cmdMetaPtr &commandMetadata);

        /**
         * @brief Retrieve module RF addressing information from database.
         *
         * @details Sends a GET request to db-service to fetch \c 'logic_address' and \c 'config' for
         *          the given module id and returns a JSON object containing the values or an error field on failure.
         *
         * @param moduleId Numeric module id to query.
         *
         * @return JSON object with fields \c `logic_address` and \c `config` on success or an
         *         \c `error` field with description on failure.
         */
        static ba::awaitable<nlohmann::json> getModuleAddressingInfo(uint moduleId);

        /**
         * @brief Post a sensor reading notification to db-service.
         *
         * @details Builds a SET notification which inserts a \c 'sensor_readings' row using a
         *          subselect to resolve sensor id and sends it as an asynchronous notification.
         *
         * @param moduleId Module id associated with the sensor.
         * @param sensorLogicId Logical sensor id within the module.
         * @param value Sensor value (numeric or textual) to store.
         * @param metadata Additional JSON metadata to store alongside the reading.
         */
        static void postSensorReading(uint moduleId, uint sensorLogicId, nlohmann::json value, nlohmann::json metadata);

        /**
         * @brief Post a log entry to the database for a module.
         *
         * @details Builds a SET notification to insert a new row into \c `logs` table and
         *          sends it asynchronously to db-service.
         *
         * @param moduleId Module id associated with the log entry.
         * @param type Log type string (e.g. "info", "error").
         * @param content Log message content.
         */
        static void postLog(uint moduleId, std::string_view type, std::string_view content);

        /**
         * @brief Update module's \c 'last_online' timestamp in database.
         *
         * @details Formats current UTC time as ISO8601 string and sends a SET request to
         *          update the \c `last_online` column for the given module id.
         *
         * @param moduleId Module id to update.
         */
        static void updateModuleLastOnline(uint moduleId);

        static ba::awaitable<void> fetchModulesConfigs();

        static ba::awaitable<void> fetchSensorsConfigs();

        static ba::awaitable<void> fetchAllConfigs();

    private:
        /**
         * @brief Send an asynchronous notification request to db-service.
         *
         * @details Finds db-service connection and posts the notification to the core worker IO context.
         *          If no db-service connection is available the notification is dropped and an error is logged.
         *
         * @param notification \c API::ApiRequest shaped as a notification to send to db-service.
         */
        static void sendNotificationToDbService(API::ApiRequest &&notification);

        /**
         * @brief Send request to db-service and await response.
         *
         * @details Finds db-service connection, posts request to core worker context,
         *          and waits asynchronously for the response while the associated command metadata remains pending.
         *          Converts the received response into an \c API::ApiResponse.
         *
         * @param request API request to send to db-service.
         * @param commandMetadata Metadata for the originating command used for timeout handling.
         *
         * @return API response from db-service or error on failure.
         */
        static ba::awaitable<API::ApiResponse> sendRequestToDbService(API::ApiRequest &&request,
                                                                      cmdMetaPtr commandMetadata);

        static ba::awaitable<API::ApiResponse> sendRequestToDbService(API::ApiRequest &&request);

        /**
         * @brief Validate incoming command parameters for database operations.
         *
         * @details Ensures the \c `params` field is present, is an object and is non-empty.
         *          On failure populates \p error.data with a error string.
         *
         * @param error Output API error object to populate on validation failure.
         * @param command Internal API command whose params are validated.
         *
         * @return true if parameters are valid, false otherwise.
         */
        static bool areParamsValid(API::ApiError &error, const API::InternalApi::Command &command);

        /**
         * @brief Prepare a request object to be sent to the database.
         *
         * @details Initializes id, method and a params object.
         *
         * @param request API request to populate.
         * @param command Command providing id and method values.
         *
         */
        static void prepareRequestToDatabase(API::ApiRequest &request,
                                             const API::InternalApi::Command &command);
    };
}
