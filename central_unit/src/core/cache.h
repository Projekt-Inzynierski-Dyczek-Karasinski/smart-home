#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace SmartHome {
    using namespace std::chrono_literals;

    /**
     * @brief Cached module configuration snapshot.
     *
     * @details Holds module metadata and last seen timestamp for fast lookup.
     */
    struct CachedModule {
        uint id; ///< Module database identifier
        uint logicAddress; ///< Module logic address used in module mediator communication
        std::string name; ///< Human-readable name
        nlohmann::json config; ///< Module configuration payload
        std::chrono::system_clock::time_point lastOnline; ///< Last online timestamp

        /**
         * @brief Serialize cached module into JSON.
         *
         * @return JSON object representing module state.
         */
        nlohmann::json to_json() const;
    };

    /**
     * @brief Cached sensor configuration snapshot.
     *
     * @details Stores sensor metadata and cache policy (TTL, enable flag).
     */
    struct CachedSensor {
        uint id; ///< Sensor database identifier
        uint logicId; ///< Sensor logic identifier within module
        uint moduleId; ///< Owning module database identifier
        std::string name; ///< Human-readable sensor name
        std::string type; ///< Sensor type string
        nlohmann::json config; ///< Sensor configuration payload

        /**
         * @brief Check if readings for this sensor should be cached.
         *
         * @return true when caching is enabled or not configured.
         */
        [[nodiscard]] bool useCache() const;

        /**
         * @brief Get cache time-to-live for this sensor.
         *
         * @return TTL duration in seconds, default when not configured.
         */
        [[nodiscard]] std::chrono::seconds cacheTTL() const;

        /**
         * @brief Serialize cached sensor into JSON.
         *
         * @return JSON object representing sensor state.
         */
        nlohmann::json to_json() const;
    };

    /**
     * @brief Cached sensor reading with freshness flag.
     */
    struct CachedReading {
        uint sensorId; ///< Owning sensor identifier
        nlohmann::json value; ///< Last recorded value
        std::chrono::system_clock::time_point timestamp; ///< Timestamp of last update
        nlohmann::json metadata; ///< Optional metadata payload
        bool stale = false; ///< Indicates TTL expiration on access

        /**
         * @brief Serialize cached reading into JSON.
         *
         * @return JSON object representing reading state.
         */
        nlohmann::json to_json() const;
    };

    /**
     * @brief Thread-safe cache for module and sensor configuration.
     *
     * @details Maintains fast lookups by id and logic address,
     *          and supports enumerating modules/sensors for runtime use.
     */
    class ConfigCache {
    public:
        /**
         * @brief Insert or update module configuration.
         *
         * @param module Module snapshot to cache.
         */
        void setModule(const CachedModule &module);

        /**
         * @brief Fetch cached module by module id.
         *
         * @param moduleId Module identifier.
         *
         * @return Cached module or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<CachedModule> getModule(uint moduleId) const;

        /**
         * @brief Retrieve all cached modules.
         *
         * @return Vector of cached modules.
         */
        [[nodiscard]] std::vector<CachedModule> getAllModules() const;

        /**
         * @brief Retrieve all cached sensors for a module.
         *
         * @param moduleId Module identifier.
         *
         * @return Vector of cached sensors for the module.
         */
        [[nodiscard]] std::vector<CachedSensor> getModuleSensors(uint moduleId) const;

        /**
         * @brief Remove cached module and its index entry.
         *
         * @param moduleId Module identifier.
         */
        void eraseModule(uint moduleId);

        /**
         * @brief Update last online timestamp for a module.
         *
         * @param moduleId Module identifier.
         * @param timestamp Timestamp to store.
         */
        void updateModuleLastOnline(uint moduleId, std::chrono::system_clock::time_point timestamp);

        /**
         * @brief Insert or update sensor configuration.
         *
         * @param sensor Sensor snapshot to cache.
         */
        void setSensor(const CachedSensor &sensor);

        /**
         * @brief Fetch cached sensor by sensor id.
         *
         * @param sensorId Sensor identifier.
         *
         * @return Cached sensor or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<CachedSensor> getSensor(uint sensorId) const;

        /**
         * @brief Retrieve all cached sensors.
         *
         * @return Vector of cached sensors.
         */
        [[nodiscard]] std::vector<CachedSensor> getAllSensors() const;

        /**
         * @brief Remove cached sensor and its index entry.
         *
         * @param sensorId Sensor identifier.
         */
        void eraseSensor(uint sensorId);

        /**
         * @brief Lookup module id by logic address.
         *
         * @param logicAddress Module logic address.
         *
         * @return Module id or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<uint> findModuleId(uint logicAddress) const;

        /**
         * @brief Lookup sensor id by module id and sensor logic id.
         *
         * @param moduleId Module identifier.
         * @param logicId Sensor logic identifier.
         *
         * @return Sensor id or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<uint> findSensorId(uint moduleId, uint logicId) const;

        /**
         * @brief Lookup sensor id by module and sensor logic address.
         *
         * @param moduleLogicAddress Module logic address.
         * @param sensorLogicId Sensor logic identifier.
         *
         * @return Sensor id or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<uint> findSensorIdByLogicAddress(uint moduleLogicAddress, uint sensorLogicId) const;

        /**
         * @brief List sensor ids for a given module.
         *
         * @param moduleId Module identifier.
         *
         * @return Vector of sensor ids.
         */
        [[nodiscard]] std::vector<uint> getSensorIdsForModule(uint moduleId) const;

        /**
         * @brief Clear all cached modules and related index.
         */
        void clearModules();

        /**
         * @brief Clear all cached sensors and related index.
         */
        void clearSensors();

        /**
         * @brief Clear modules and sensors together.
         */
        void clear();

        /**
         * @brief Number of cached modules.
         */
        [[nodiscard]] size_t modulesSize() const;

        /**
         * @brief Number of cached sensors.
         */
        [[nodiscard]] size_t sensorsSize() const;

    private:
        mutable std::shared_mutex mMutex;

        std::unordered_map<uint, CachedModule> mModules; ///< moduleId: module
        std::unordered_map<uint, CachedSensor> mSensors; ///< sensorId: sensor

        std::unordered_map<uint, uint> mModulesIndex; ///< logicAddress: moduleId
        std::map<std::pair<uint, uint>, uint> mSensorsIndex; ///< (moduleId, logicId): sensorId

        /**
         * @brief Add module entry to logic address index.
         *
         * @param module Module snapshot to index.
         */
        void addToModulesIndex(const CachedModule &module);

        /**
         * @brief Remove module entry from logic address index.
         *
         * @param logicAddress Logic address of the module to remove from index.
         */
        void removeFromModulesIndex(uint logicAddress);

        /**
         * @brief Add sensor entry to module/logic index.
         *
         * @param sensor Sensor snapshot to index.
         */
        void addToSensorsIndex(const CachedSensor &sensor);

        /**
         * @brief Remove sensor entry from module/logic index.
         *
         * @param moduleId Module identifier of the sensor to remove from index.
         * @param logicId Logic identifier of the sensor to remove from index.
         */
        void removeFromSensorsIndex(uint moduleId, uint logicId);
    };

    /**
     * @brief Thread-safe cache for sensor readings with TTL freshness.
     *
     * @details Uses ConfigCache to resolve per-sensor caching policy.
     *          Returned readings can be marked stale when TTL expires.
     */
    class ReadingsCache {
    public:
        /**
         * @brief Construct readings cache with config cache reference.
         *
         * @param configCache Configuration cache used for TTL lookup.
         */
        explicit ReadingsCache(const ConfigCache &configCache);

        /**
         * @brief Fetch cached reading by sensor id.
         *
         * @details Returns cached reading even if stale (flagged via CachedReading::stale).
         *
         * @param sensorId Sensor identifier.
         *
         * @return Cached reading or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<CachedReading> get(uint sensorId) const;

        /**
         * @brief Fetch only fresh cached reading by sensor id.
         *
         * @param sensorId Sensor identifier.
         *
         * @return Cached reading if within TTL, std::nullopt otherwise.
         */
        [[nodiscard]] std::optional<CachedReading> getFresh(uint sensorId) const;

        /**
         * @brief Retrieve all cached readings for debugging.
         *
         * @return Vector of cached readings (no freshness filtering).
         */
        [[nodiscard]] std::vector<CachedReading> getDebugAll() const;

        /**
         * @brief Insert or update sensor reading.
         *
         * @param sensorId Sensor identifier.
         * @param value Reading value.
         * @param metadata Reading metadata.
         */
        void set(uint sensorId, const nlohmann::json &value, const nlohmann::json &metadata);

        /**
         * @brief Remove cached reading for a sensor.
         *
         * @param sensorId Sensor identifier.
         */
        void erase(uint sensorId);

        /**
         * @brief Clear all cached readings.
         */
        void clear();

        /**
         * @brief Number of cached readings.
         */
        [[nodiscard]] size_t size() const;

    private:
        mutable std::shared_mutex mMutex;

        const ConfigCache &mConfigCache;

        std::unordered_map<uint, CachedReading> mReadings;

        /**
         * @brief Resolve TTL for given sensor id using config cache.
         */
        [[nodiscard]] std::chrono::seconds getTTL(uint sensorId) const;

        /**
         * @brief Check if reading is within TTL window.
         */
        [[nodiscard]] static bool isFresh(const CachedReading &reading, std::chrono::seconds ttl);
    };
}
