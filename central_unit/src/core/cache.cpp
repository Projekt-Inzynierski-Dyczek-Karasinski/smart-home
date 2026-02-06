#include "cache.h"

#include <mutex>

namespace SmartHome {
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

    std::optional<CachedModule> ConfigCache::getModule(const int moduleId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return std::nullopt;
        return iter->second;
    }

    void ConfigCache::eraseModule(const int moduleId) {
        std::unique_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return;

        mModules.erase(iter);
        removeFromModulesIndex(iter->second.logicAddress);
    }

    void ConfigCache::updateModuleLastOnline(const int moduleId,
                                             const std::chrono::steady_clock::time_point timestamp) {
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

    std::optional<CachedSensor> ConfigCache::getSensor(const int sensorId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mSensors.find(sensorId);
        if (iter == mSensors.end()) return std::nullopt;
        return iter->second;
    }

    void ConfigCache::eraseSensor(const int sensorId) {
        std::unique_lock lock(mMutex);

        const auto iter = mSensors.find(sensorId);
        if (iter == mSensors.end()) return;

        mSensors.erase(iter);
        removeFromSensorsIndex(iter->second.moduleId, iter->second.logicId);
    }

    std::optional<int> ConfigCache::findModuleId(const int logicAddress) const {
        std::shared_lock lock(mMutex);

        const auto iter = mModulesIndex.find(logicAddress);
        if (iter == mModulesIndex.end()) return std::nullopt;
        return iter->second;
    }

    std::optional<int> ConfigCache::findSensorId(const int moduleId, const int logicId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mSensorsIndex.find({moduleId, logicId});
        if (iter == mSensorsIndex.end()) return std::nullopt;
        return iter->second;
    }

    std::optional<int> ConfigCache::findSensorIdByLogicAddress(const int moduleLogicAddress,
                                                               const int sensorLogicId) const {
        std::shared_lock lock(mMutex);

        const auto moduleIter = mModulesIndex.find(moduleLogicAddress);
        if (moduleIter == mModulesIndex.end()) return std::nullopt;

        const int moduleId = moduleIter->second;
        const auto sensorIter = mSensorsIndex.find({moduleId, sensorLogicId});
        if (sensorIter == mSensorsIndex.end()) return std::nullopt;

        return sensorIter->second;
    }

    std::vector<int> ConfigCache::getSensorIdsForModule(const int moduleId) const {
        std::shared_lock lock(mMutex);

        std::vector<int> sensorIds;

        for (const auto &[key, sensorId]: mSensorsIndex) {
            if (key.first == moduleId) {
                sensorIds.push_back(sensorId);
            }
        }
        return sensorIds;
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

    void ConfigCache::removeFromModulesIndex(const int logicAddress) {
        mModulesIndex.erase(logicAddress);
    }

    void ConfigCache::addToSensorsIndex(const CachedSensor &sensor) {
        mSensorsIndex[{sensor.moduleId, sensor.logicId}] = sensor.id;
    }

    void ConfigCache::removeFromSensorsIndex(int moduleId, int logicId) {
        mSensorsIndex.erase({moduleId, logicId});
    }


    ReadingsCache::ReadingsCache(const ConfigCache &configCache) : mConfigCache(configCache) {
    }

    std::optional<CachedReading> ReadingsCache::get(const int sensorId) const {
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

    std::optional<CachedReading> ReadingsCache::getFresh(const int sensorId) const {
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

    void ReadingsCache::set(const int sensorId, const nlohmann::json &value) {
        CachedReading reading{
            .sensorId = sensorId,
            .value = value,
            .timestamp = std::chrono::steady_clock::now(),
            .stale = false
        };
        std::unique_lock lock(mMutex);
        mReadings.insert_or_assign(sensorId, std::move(reading));
    }

    void ReadingsCache::erase(const int sensorId) {
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

    std::chrono::seconds ReadingsCache::getTTL(const int sensorId) const {
        const auto sensor = mConfigCache.getSensor(sensorId);
        if (sensor.has_value() && sensor->useCache()) {
            return sensor->cacheTTL();
        }
        return 60s; // Default TTL if sensor not found or caching disabled
    }

    bool ReadingsCache::isFresh(const CachedReading &reading, const std::chrono::seconds ttl) {
        const auto age = std::chrono::steady_clock::now() - reading.timestamp;
        return age <= ttl;
    }
}
