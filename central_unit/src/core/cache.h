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

    // TODO add database query cache
    // TODO (optional) optimize database queries to limit their size

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
        std::optional<std::chrono::system_clock::time_point> lastOnline; ///< Last online timestamp
        bool stale = false; ///< Module object has changed, update pending

        /**
         * @brief TODO !pr
         *
         * @return
         */
        [[nodiscard]] bool isFresh() const;

        /**
         * @brief Serialize cached module into JSON.
         *
         * @return JSON object representing module state.
         */
        nlohmann::json to_json() const;
    };

    /**
     * @brief Cached device configuration snapshot.
     *
     * @details Stores device metadata and cache policy (TTL, enable flag).
     */
    struct CachedDevice {
        uint id; ///< Device database identifier
        uint logicId; ///< Device logic identifier within module
        uint moduleId; ///< Owning module database identifier
        std::string name; ///< Human-readable device name
        std::string type; ///< Device type string
        nlohmann::json config; ///< Device configuration payload
        bool stale = false; ///< Device object has changed, update pending

        /**
         * @brief Check if readings for this device should be cached.
         *
         * @return true when caching is enabled or not configured.
         */
        [[nodiscard]] bool useCache() const;

        /**
         * @brief Get cache time-to-live for this device.
         *
         * @return TTL duration in seconds, default when not configured.
         */
        [[nodiscard]] std::chrono::seconds cacheTTL() const;

        /**
         * @brief TODO !pr
         *
         * @return
         */
        [[nodiscard]] bool isFresh() const;

        /**
         * @brief Serialize cached device into JSON.
         *
         * @return JSON object representing device state.
         */
        nlohmann::json to_json() const;
    };

    /**
     * @brief Cached device reading with freshness flag.
     */
    struct CachedReading {
        uint deviceId; ///< Owning device identifier
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
     * @brief Thread-safe cache for module and device configuration.
     *
     * @details Maintains fast lookups by id and logic address,
     *          and supports enumerating modules/devices for runtime use.
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
         * @param isFresh TODO !pr
         *
         * @return Cached module or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<CachedModule> getModule(uint moduleId, bool isFresh = false) const;

        std::optional<bool> compareExchangeIsModuleFresh(uint moduleId, bool expected, bool desired);

        /**
         * @brief Retrieve all cached modules.
         *
         * @return Vector of cached modules.
         */
        [[nodiscard]] std::vector<CachedModule> getAllModules() const;

        /**
         * @brief Retrieve all cached devices for a module.
         *
         * @param moduleId Module identifier.
         *
         * @return Vector of cached devices for the module.
         */
        [[nodiscard]] std::vector<CachedDevice> getModuleDevices(uint moduleId) const;

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
         * @brief Insert or update device configuration.
         *
         * @param device Device snapshot to cache.
         */
        void setDevice(const CachedDevice &device);

        /**
         * @brief Fetch cached device by device id.
         *
         * @param deviceId Device identifier.
         * @param isFresh TODO !pr
         *
         * @return Cached device or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<CachedDevice> getDevice(uint deviceId, bool isFresh = false) const;


        std::optional<bool> compareExchangeIsDeviceFresh(uint deviceId, bool expected, bool desired);

        /**
         * @brief Retrieve all cached devices.
         *
         * @return Vector of cached devices.
         */
        [[nodiscard]] std::vector<CachedDevice> getAllDevices() const;

        /**
         * @brief Remove cached device and its index entry.
         *
         * @param deviceId Device identifier.
         */
        void eraseDevice(uint deviceId);

        /**
         * @brief Lookup module id by logic address.
         *
         * @param logicAddress Module logic address.
         *
         * @return Module id or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<uint> findModuleId(uint logicAddress) const;

        /**
         * @brief Lookup device id by module id and device logic id.
         *
         * @param moduleId Module identifier.
         * @param logicId Device logic identifier.
         *
         * @return Device id or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<uint> findDeviceId(uint moduleId, uint logicId) const;

        /**
         * @brief Lookup device id by module and device logic address.
         *
         * @param moduleLogicAddress Module logic address.
         * @param deviceLogicId Device logic identifier.
         *
         * @return Device id or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<uint> findDeviceIdByLogicAddress(uint moduleLogicAddress, uint deviceLogicId) const;

        /**
         * @brief List device ids for a given module.
         *
         * @param moduleId Module identifier.
         *
         * @return Vector of device ids.
         */
        [[nodiscard]] std::vector<uint> getDeviceIdsForModule(uint moduleId) const;

        /**
         * @brief Clear all cached modules and related index.
         */
        void clearModules();

        /**
         * @brief Clear all cached devices and related index.
         */
        void clearDevices();

        /**
         * @brief Clear modules and devices together.
         */
        void clear();

        /**
         * @brief Number of cached modules.
         */
        [[nodiscard]] size_t modulesSize() const;

        /**
         * @brief Number of cached devices.
         */
        [[nodiscard]] size_t devicesSize() const;

    private:
        mutable std::shared_mutex mMutex;

        std::unordered_map<uint, CachedModule> mModules; ///< moduleId: module
        std::unordered_map<uint, CachedDevice> mDevices; ///< deviceId: device

        std::unordered_map<uint, uint> mModulesIndex; ///< logicAddress: moduleId
        std::map<std::pair<uint, uint>, uint> mDevicesIndex; ///< (moduleId, logicId): deviceId

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
         * @brief Add device entry to module/logic index.
         *
         * @param device Device snapshot to index.
         */
        void addToDevicesIndex(const CachedDevice &device);

        /**
         * @brief Remove device entry from module/logic index.
         *
         * @param moduleId Module identifier of the device to remove from index.
         * @param logicId Logic identifier of the device to remove from index.
         */
        void removeFromDevicesIndex(uint moduleId, uint logicId);
    };

    /**
     * @brief Thread-safe cache for device readings with TTL freshness.
     *
     * @details Uses ConfigCache to resolve per-device caching policy.
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
         * @brief Fetch cached reading by device id.
         *
         * @details Returns cached reading even if stale (flagged via CachedReading::stale).
         *
         * @param deviceId Device identifier.
         *
         * @return Cached reading or std::nullopt if missing.
         */
        [[nodiscard]] std::optional<CachedReading> get(uint deviceId) const;

        /**
         * @brief Fetch only fresh cached reading by device id.
         *
         * @param deviceId Device identifier.
         *
         * @return Cached reading if within TTL, std::nullopt otherwise.
         */
        [[nodiscard]] std::optional<CachedReading> getFresh(uint deviceId) const;

        /**
         * @brief Retrieve all cached readings for debugging.
         *
         * @return Vector of cached readings (no freshness filtering).
         */
        [[nodiscard]] std::vector<CachedReading> getDebugAll() const;

        /**
         * @brief Insert or update device reading.
         *
         * @param deviceId Device identifier.
         * @param value Reading value.
         * @param metadata Reading metadata.
         */
        void set(uint deviceId, const nlohmann::json &value, const nlohmann::json &metadata);

        /**
         * @brief Remove cached reading for a device.
         *
         * @param deviceId Device identifier.
         */
        void erase(uint deviceId);

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
         * @brief Resolve TTL for given device id using config cache.
         */
        [[nodiscard]] std::chrono::seconds getTTL(uint deviceId) const;

        /**
         * @brief Check if reading is within TTL window.
         */
        [[nodiscard]] static bool isFresh(const CachedReading &reading, std::chrono::seconds ttl);
    };
}
