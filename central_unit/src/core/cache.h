#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace SmartHome {
    using namespace std::chrono_literals;

    struct CachedModule {
        uint id;
        uint logicAddress;
        std::string name;
        nlohmann::json config;
        std::chrono::system_clock::time_point lastOnline;
    };

    struct CachedSensor {
        uint id;
        uint logicId;
        uint moduleId;
        std::string name;
        std::string type;
        nlohmann::json config;

        [[nodiscard]] bool useCache() const;

        [[nodiscard]] std::chrono::seconds cacheTTL() const;
    };

    struct CachedReading {
        int sensorId;
        nlohmann::json value;
        std::chrono::system_clock::time_point timestamp;
        bool stale = false;
    };

    class ConfigCache {
    public:
        void setModule(const CachedModule &module);

        [[nodiscard]] std::optional<CachedModule> getModule(int moduleId) const;

        void eraseModule(int moduleId);

        void updateModuleLastOnline(int moduleId, std::chrono::system_clock::time_point timestamp);

        void setSensor(const CachedSensor &sensor);

        [[nodiscard]] std::optional<CachedSensor> getSensor(int sensorId) const;


        void eraseSensor(int sensorId);

        [[nodiscard]] std::optional<int> findModuleId(int logicAddress) const;

        [[nodiscard]] std::optional<int> findSensorId(int moduleId, int logicId) const;

        [[nodiscard]] std::optional<int> findSensorIdByLogicAddress(int moduleLogicAddress, int sensorLogicId) const;

        [[nodiscard]] std::vector<int> getSensorIdsForModule(int moduleId) const;

        void clear();

        [[nodiscard]] size_t modulesSize() const;

        [[nodiscard]] size_t sensorsSize() const;

    private:
        mutable std::shared_mutex mMutex;

        std::unordered_map<int, CachedModule> mModules; ///< moduleId: module
        std::unordered_map<int, CachedSensor> mSensors; ///< sensorId: sensor

        std::unordered_map<int, int> mModulesIndex; ///< logicAddress: moduleId
        std::map<std::pair<int, int>, int> mSensorsIndex; ///< (moduleId, logicId): sensorId

        void addToModulesIndex(const CachedModule &module);

        void removeFromModulesIndex(int logicAddress);

        void addToSensorsIndex(const CachedSensor &sensor);

        void removeFromSensorsIndex(int moduleId, int logicId);
    };

    class ReadingsCache {
    public:
        explicit ReadingsCache(const ConfigCache &configCache);

        [[nodiscard]] std::optional<CachedReading> get(int sensorId) const;

        [[nodiscard]] std::optional<CachedReading> getFresh(int sensorId) const;

        void set(int sensorId, const nlohmann::json &value);

        void erase(int sensorId);

        void clear();

        [[nodiscard]] size_t size() const;

    private:
        mutable std::shared_mutex mMutex;

        const ConfigCache &mConfigCache;

        std::unordered_map<int, CachedReading> mReadings;

        [[nodiscard]] std::chrono::seconds getTTL(int sensorId) const;

        [[nodiscard]] static bool isFresh(const CachedReading &reading, std::chrono::seconds ttl);
    };
}
