#include "cache.h"
#include "utils.h"

#include <mutex>


namespace SmartHome {
    bool CachedModule::isFresh() const {
        return !stale;
    }

    nlohmann::json CachedModule::to_json() const {
        nlohmann::json json;
        json["id"] = id;
        json["logic_address"] = logicAddress;
        json["name"] = name;
        json["config"] = config;
        if (lastOnline.has_value()) {
            json["last_online"] = Utils::timePointToTimestampTz(lastOnline.value());
        } else {
            json["last_online"] = nullptr;
        }
        return json;
    }

    bool CachedDevice::useCache() const {
        if (config.contains("use_cache") && config["use_cache"].is_boolean()) {
            return config["use_cache"].get<bool>();
        }
        return true; // Default to using cache if not specified
    }

    std::chrono::seconds CachedDevice::cacheTTL() const {
        if (config.contains("cache_ttl") && config["cache_ttl"].is_number_unsigned()) {
            return std::chrono::seconds(config["cache_ttl"].get<uint64_t>());
        }
        return 60s; // Default TTL of 60 seconds
    }

    bool CachedDevice::isFresh() const {
        return !stale;
    }

    nlohmann::json CachedDevice::to_json() const {
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
        json["device_id"] = deviceId;
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

    std::optional<CachedModule> ConfigCache::getModule(const uint moduleId, const bool isFresh) const {
        std::shared_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return std::nullopt;
        const auto &module = iter->second;

        if (isFresh && !module.isFresh()) return std::nullopt;
        return module;
    }

    std::optional<bool> ConfigCache::compareExchangeIsModuleFresh(const uint moduleId,
                                                                  const bool expected,
                                                                  const bool desired) {
        std::unique_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return std::nullopt;
        auto &module = iter->second;

        const bool result = expected == module.isFresh();
        module.stale = !desired; // Inverses isFresh and stale logic - desired isFresh = false means module is stale

        return result;
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

    std::vector<CachedDevice> ConfigCache::getModuleDevices(const uint moduleId) const {
        const auto devicesIds = getDeviceIdsForModule(moduleId);

        std::vector<CachedDevice> devices;
        devices.reserve(devicesIds.size());
        for (const auto deviceId: devicesIds) {
            const auto deviceOpt = getDevice(deviceId);
            if (deviceOpt.has_value()) {
                devices.push_back(deviceOpt.value());
            }
        }

        return devices;
    }

    void ConfigCache::eraseModule(const uint moduleId) {
        std::unique_lock lock(mMutex);

        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return;

        mModules.erase(iter);
        removeFromModulesIndex(iter->second.logicAddress);
    }

    void ConfigCache::updateModuleLastOnline(const uint moduleId,
                                             const std::chrono::system_clock::time_point timestamp) {
        std::unique_lock lock(mMutex);
        const auto iter = mModules.find(moduleId);
        if (iter == mModules.end()) return;

        iter->second.lastOnline = timestamp;
    }

    void ConfigCache::setDevice(const CachedDevice &device) {
        std::unique_lock lock(mMutex);

        // Remove old entry if device with same ID already exists
        const auto iter = mDevices.find(device.id);
        if (iter != mDevices.end()) {
            removeFromDevicesIndex(iter->second.moduleId, iter->second.logicId);
        }

        mDevices.insert_or_assign(device.id, device);
        addToDevicesIndex(device);
    }

    std::optional<CachedDevice> ConfigCache::getDevice(const uint deviceId, const bool isFresh) const {
        std::shared_lock lock(mMutex);

        const auto iter = mDevices.find(deviceId);
        if (iter == mDevices.end()) return std::nullopt;
        auto device = iter->second;

        if (isFresh && !device.isFresh()) return std::nullopt;
        return device;
    }

    std::optional<bool> ConfigCache::compareExchangeIsDeviceFresh(const uint deviceId,
                                                                  const bool expected,
                                                                  const bool desired) {
        std::unique_lock lock(mMutex);

        const auto iter = mDevices.find(deviceId);
        if (iter == mDevices.end()) return std::nullopt;
        auto &device = iter->second;

        const bool result = expected == device.isFresh();
        device.stale = !desired; // Inverses isFresh and stale logic - desired (isFresh) = false means module is stale

        return result;
    }

    std::vector<CachedDevice> ConfigCache::getAllDevices() const {
        std::shared_lock lock(mMutex);

        std::vector<CachedDevice> devices;
        devices.reserve(mDevices.size());
        for (const auto &device: mDevices | std::views::values) {
            devices.push_back(device);
        }
        return devices;
    }

    void ConfigCache::eraseDevice(const uint deviceId) {
        std::unique_lock lock(mMutex);

        const auto iter = mDevices.find(deviceId);
        if (iter == mDevices.end()) return;

        mDevices.erase(iter);
        removeFromDevicesIndex(iter->second.moduleId, iter->second.logicId);
    }

    std::optional<uint> ConfigCache::findModuleId(const uint logicAddress) const {
        std::shared_lock lock(mMutex);

        const auto iter = mModulesIndex.find(logicAddress);
        if (iter == mModulesIndex.end()) return std::nullopt;
        return iter->second;
    }

    std::optional<uint> ConfigCache::findDeviceId(const uint moduleId, const uint logicId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mDevicesIndex.find({moduleId, logicId});
        if (iter == mDevicesIndex.end()) return std::nullopt;
        return iter->second;
    }

    std::optional<uint> ConfigCache::findDeviceIdByLogicAddress(const uint moduleLogicAddress,
                                                                const uint deviceLogicId) const {
        std::shared_lock lock(mMutex);

        const auto moduleIter = mModulesIndex.find(moduleLogicAddress);
        if (moduleIter == mModulesIndex.end()) return std::nullopt;

        const auto moduleId = moduleIter->second;
        const auto deviceIter = mDevicesIndex.find({moduleId, deviceLogicId});
        if (deviceIter == mDevicesIndex.end()) return std::nullopt;

        return deviceIter->second;
    }

    std::vector<uint> ConfigCache::getDeviceIdsForModule(const uint moduleId) const {
        std::shared_lock lock(mMutex);

        std::vector<uint> deviceIds;

        for (const auto &[key, deviceId]: mDevicesIndex) {
            if (key.first == moduleId) {
                deviceIds.push_back(deviceId);
            }
        }
        return deviceIds;
    }

    void ConfigCache::clearModules() {
        std::unique_lock lock(mMutex);
        mModules.clear();
        mModulesIndex.clear();
    }

    void ConfigCache::clearDevices() {
        std::unique_lock lock(mMutex);
        mDevices.clear();
        mDevicesIndex.clear();
    }

    void ConfigCache::clear() {
        std::unique_lock lock(mMutex);
        mModules.clear();
        mDevices.clear();
        mModulesIndex.clear();
        mDevicesIndex.clear();
    }

    size_t ConfigCache::modulesSize() const {
        std::shared_lock lock(mMutex);
        return mModules.size();
    }

    size_t ConfigCache::devicesSize() const {
        std::shared_lock lock(mMutex);
        return mDevices.size();
    }


    void ConfigCache::addToModulesIndex(const CachedModule &module) {
        mModulesIndex[module.logicAddress] = module.id;
    }

    void ConfigCache::removeFromModulesIndex(const uint logicAddress) {
        mModulesIndex.erase(logicAddress);
    }

    void ConfigCache::addToDevicesIndex(const CachedDevice &device) {
        mDevicesIndex[{device.moduleId, device.logicId}] = device.id;
    }

    void ConfigCache::removeFromDevicesIndex(const uint moduleId, const uint logicId) {
        mDevicesIndex.erase({moduleId, logicId});
    }


    ReadingsCache::ReadingsCache(const ConfigCache &configCache) : mConfigCache(configCache) {
    }

    std::optional<CachedReading> ReadingsCache::get(const uint deviceId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mReadings.find(deviceId);
        if (iter == mReadings.end()) return std::nullopt;

        // Copy reading before releasing lock
        auto reading = iter->second;
        lock.unlock();

        // Check if reading is fresh based on device's TTL
        const auto ttl = getTTL(deviceId);
        if (!isFresh(reading, ttl)) {
            reading.stale = true;
        }

        return reading;
    }

    std::optional<CachedReading> ReadingsCache::getFresh(const uint deviceId) const {
        std::shared_lock lock(mMutex);

        const auto iter = mReadings.find(deviceId);
        if (iter == mReadings.end()) return std::nullopt;

        // Copy reading before releasing lock
        const auto &reading = iter->second;
        lock.unlock();

        // Check if reading is fresh based on device's TTL
        const auto ttl = getTTL(deviceId);
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

    void ReadingsCache::set(const uint deviceId, const nlohmann::json &value, const nlohmann::json &metadata) {
        CachedReading reading{
            .deviceId = deviceId,
            .value = value,
            .timestamp = std::chrono::system_clock::now(),
            .metadata = metadata,
            .stale = false
        };
        std::unique_lock lock(mMutex);
        mReadings.insert_or_assign(deviceId, std::move(reading));
    }

    void ReadingsCache::erase(const uint deviceId) {
        std::unique_lock lock(mMutex);
        mReadings.erase(deviceId);
    }

    void ReadingsCache::clear() {
        std::unique_lock lock(mMutex);
        mReadings.clear();
    }

    size_t ReadingsCache::size() const {
        std::shared_lock lock(mMutex);
        return mReadings.size();
    }

    std::chrono::seconds ReadingsCache::getTTL(const uint deviceId) const {
        const auto device = mConfigCache.getDevice(deviceId);
        if (device.has_value() && device->useCache()) {
            return device->cacheTTL();
        }
        return 0s; // 0 TTL if device not found or caching disabled
    }

    bool ReadingsCache::isFresh(const CachedReading &reading, const std::chrono::seconds ttl) {
        const auto age = std::chrono::system_clock::now() - reading.timestamp;
        return age < ttl;
    }
}
