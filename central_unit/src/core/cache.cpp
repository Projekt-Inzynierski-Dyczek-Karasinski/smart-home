#include "cache.h"
#include "utils.h"

#include <mutex>


namespace SmartHome {
    nlohmann::json CachedModule::to_json() const {
        nlohmann::json json;
        json["id"] = id;
        json["logic_address"] = logicAddress;
        json["name"] = name;
        json["config"] = config;
        json["last_online"] = Utils::timePointToTimestampTz(lastOnline);
        return json;
    }

    bool CachedSensor::useCache() const {
        if (config.contains("use_cache") && config["use_cache"].is_boolean()) {
            return config["use_cache"].get<bool>();
        }
        return true; // Default to using cache if not specified
    }

    std::chrono::seconds CachedSensor::cacheTTL() const {
        if (config.contains("cache_ttl") && config["cache_ttl"].is_number_unsigned()) {
            return std::chrono::seconds(config["cache_ttl"].get<uint64_t>());
        }
        return 60s; // Default TTL of 60 seconds
    }

    nlohmann::json CachedSensor::to_json() const {
        nlohmann::json json;
        json["id"] = id;
        json["logic_id"] = logicId;
        json["module_id"] = moduleId;
        json["name"] = name;
        json["type"] = type;
        json["config"] = config;
        return json;
    }

    nlohmann::json CachedReading::to_json() const {
        nlohmann::json json;
        json["sensor_id"] = sensorId;
        json["value"] = value;
        json["timestamp"] = Utils::timePointToTimestampTz(timestamp);
        json["metadata"] = metadata;
        json["stale"] = stale;
        return json;
    }

    void ConfigCache::setModule(const CachedModule &module) {
        std::unique_lock lock(mMutex);

        // Remove old entry if module with same ID already exists
        const auto iter = mModules.find(module.id);
        if (iter != mModules.end()) {
            removeFromModulesIndex(iter->second.logicAddress);
        }

        mModules.insert_or_assign(module.id, module);
        addToModulesIndex(module);
    }

    std::optional<CachedModule> ConfigCache::getModule(const uint moduleId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return std::nullopt;
        return iter->second;
    }

    std::vector<CachedModule> ConfigCache::getAllModules() const {
        std::shared_lock lock(mMutex);

        std::vector<CachedModule> modules;
        modules.reserve(mModules.size());
        for (const auto &module: mModules | std::views::values) {
            modules.push_back(module);
        }
        return modules;
    }

    std::vector<CachedSensor> ConfigCache::getModuleSensors(const uint moduleId) const {
        const auto sensorsIds = getSensorIdsForModule(moduleId);

        std::vector<CachedSensor> sensors;
        sensors.reserve(sensorsIds.size());
        for (const auto sensorId: sensorsIds) {
            const auto sensorOpt = getSensor(sensorId);
            if (sensorOpt.has_value()) {
                sensors.push_back(sensorOpt.value());
            }
        }

        return sensors;
    }

    void ConfigCache::eraseModule(uint moduleId) {
        std::unique_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return;

        mModules.erase(iter);
        removeFromModulesIndex(iter->second.logicAddress);
    }

    void ConfigCache::updateModuleLastOnline(uint moduleId,
                                             const std::chrono::system_clock::time_point timestamp) {
        std::unique_lock lock(mMutex);
        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return;

        iter->second.lastOnline = timestamp;
    }

    void ConfigCache::setSensor(const CachedSensor &sensor) {
        std::unique_lock lock(mMutex);

        // Remove old entry if sensor with same ID already exists
        const auto iter = mSensors.find(sensor.id);
        if (iter != mSensors.end()) {
            removeFromSensorsIndex(iter->second.moduleId, iter->second.logicId);
        }

        mSensors.insert_or_assign(sensor.id, sensor);
        addToSensorsIndex(sensor);
    }

    std::optional<CachedSensor> ConfigCache::getSensor(uint sensorId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mSensors.find(sensorId);
        if (iter == mSensors.end()) return std::nullopt;
        return iter->second;
    }

    std::vector<CachedSensor> ConfigCache::getAllSensors() const {
        std::shared_lock lock(mMutex);

        std::vector<CachedSensor> sensors;
        sensors.reserve(mSensors.size());
        for (const auto &sensor: mSensors | std::views::values) {
            sensors.push_back(sensor);
        }
        return sensors;
    }

    void ConfigCache::eraseSensor(uint sensorId) {
        std::unique_lock lock(mMutex);

        const auto iter = mSensors.find(sensorId);
        if (iter == mSensors.end()) return;

        mSensors.erase(iter);
        removeFromSensorsIndex(iter->second.moduleId, iter->second.logicId);
    }

    std::optional<uint> ConfigCache::findModuleId(uint logicAddress) const {
        std::shared_lock lock(mMutex);

        const auto iter = mModulesIndex.find(logicAddress);
        if (iter == mModulesIndex.end()) return std::nullopt;
        return iter->second;
    }

    std::optional<uint> ConfigCache::findSensorId(uint moduleId, uint logicId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mSensorsIndex.find({moduleId, logicId});
        if (iter == mSensorsIndex.end()) return std::nullopt;
        return iter->second;
    }

    std::optional<uint> ConfigCache::findSensorIdByLogicAddress(uint moduleLogicAddress,
                                                                uint sensorLogicId) const {
        std::shared_lock lock(mMutex);

        const auto moduleIter = mModulesIndex.find(moduleLogicAddress);
        if (moduleIter == mModulesIndex.end()) return std::nullopt;

        const int moduleId = moduleIter->second;
        const auto sensorIter = mSensorsIndex.find({moduleId, sensorLogicId});
        if (sensorIter == mSensorsIndex.end()) return std::nullopt;

        return sensorIter->second;
    }

    std::vector<uint> ConfigCache::getSensorIdsForModule(uint moduleId) const {
        std::shared_lock lock(mMutex);

        std::vector<uint> sensorIds;

        for (const auto &[key, sensorId]: mSensorsIndex) {
            if (key.first == moduleId) {
                sensorIds.push_back(sensorId);
            }
        }
        return sensorIds;
    }

    void ConfigCache::clearModules() {
        std::unique_lock lock(mMutex);
        mModules.clear();
        mModulesIndex.clear();
    }

    void ConfigCache::clearSensors() {
        std::unique_lock lock(mMutex);
        mSensors.clear();
        mSensorsIndex.clear();
    }

    void ConfigCache::clear() {
        std::unique_lock lock(mMutex);
        mModules.clear();
        mSensors.clear();
        mModulesIndex.clear();
        mSensorsIndex.clear();
    }

    size_t ConfigCache::modulesSize() const {
        std::shared_lock lock(mMutex);
        return mModules.size();
    }

    size_t ConfigCache::sensorsSize() const {
        std::shared_lock lock(mMutex);
        return mSensors.size();
    }


    void ConfigCache::addToModulesIndex(const CachedModule &module) {
        mModulesIndex[module.logicAddress] = module.id;
    }

    void ConfigCache::removeFromModulesIndex(uint logicAddress) {
        mModulesIndex.erase(logicAddress);
    }

    void ConfigCache::addToSensorsIndex(const CachedSensor &sensor) {
        mSensorsIndex[{sensor.moduleId, sensor.logicId}] = sensor.id;
    }

    void ConfigCache::removeFromSensorsIndex(uint moduleId, uint logicId) {
        mSensorsIndex.erase({moduleId, logicId});
    }


    ReadingsCache::ReadingsCache(const ConfigCache &configCache) : mConfigCache(configCache) {
    }

    std::optional<CachedReading> ReadingsCache::get(uint sensorId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mReadings.find(sensorId);
        if (iter == mReadings.end()) return std::nullopt;

        // Copy reading before releasing lock
        auto reading = iter->second;
        lock.unlock();

        // Check if reading is fresh based on sensor's TTL
        const auto ttl = getTTL(sensorId);
        if (!isFresh(reading, ttl)) {
            reading.stale = true;
        }

        return reading;
    }

    std::optional<CachedReading> ReadingsCache::getFresh(const uint sensorId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mReadings.find(sensorId);
        if (iter == mReadings.end()) return std::nullopt;

        // Copy reading before releasing lock
        const auto &reading = iter->second;
        lock.unlock();

        // Check if reading is fresh based on sensor's TTL
        const auto ttl = getTTL(sensorId);
        if (!isFresh(reading, ttl)) return std::nullopt; // Not fresh, treat as not found

        return reading;
    }

    std::vector<CachedReading> ReadingsCache::getDebugAll() const {
        std::shared_lock lock(mMutex);

        std::vector<CachedReading> readings;
        readings.reserve(mReadings.size());
        for (const auto &reading: mReadings | std::views::values) {
            readings.push_back(reading);
        }
        return readings;
    }

    void ReadingsCache::set(const uint sensorId, const nlohmann::json &value, const nlohmann::json &metadata) {
        CachedReading reading{
            .sensorId = sensorId,
            .value = value,
            .timestamp = std::chrono::system_clock::now(),
            .metadata = metadata,
            .stale = false
        };
        std::unique_lock lock(mMutex);
        mReadings.insert_or_assign(sensorId, std::move(reading));
    }

    void ReadingsCache::erase(const uint sensorId) {
        std::unique_lock lock(mMutex);
        mReadings.erase(sensorId);
    }

    void ReadingsCache::clear() {
        std::unique_lock lock(mMutex);
        mReadings.clear();
    }

    size_t ReadingsCache::size() const {
        std::shared_lock lock(mMutex);
        return mReadings.size();
    }

    std::chrono::seconds ReadingsCache::getTTL(const uint sensorId) const {
        const auto sensor = mConfigCache.getSensor(sensorId);
        if (sensor.has_value() && sensor->useCache()) {
            return sensor->cacheTTL();
        }
        return 0s; // 0 TTL if sensor not found or caching disabled
    }

    bool ReadingsCache::isFresh(const CachedReading &reading, const std::chrono::seconds ttl) {
        const auto age = std::chrono::system_clock::now() - reading.timestamp;
        return age < ttl;
    }
}
