#include "cache.h"
#include "utils.h"
#include "constants.h"

#include <mutex>
#include <utility>


namespace SmartHome {
    using namespace std::string_literals;

    namespace cdbi = Constants::DatabaseIdentifiers;
    namespace cdck = Constants::DeviceConfigKeys;
    namespace cc = Constants::Common;

    CachedModule::CachedModule(const uint id,
                               const uint logicAddress,
                               std::string name,
                               nlohmann::json config,
                               const std::optional<std::chrono::system_clock::time_point> lastOnline)
        : id(id), logicAddress(logicAddress), name(std::move(name)), config(std::move(config)), lastOnline(lastOnline) {
    }

    CachedModule::CachedModule(const nlohmann::json &moduleData) {
        if (!moduleData.contains(cdbi::ID) ||
            !moduleData[cdbi::ID].is_number_integer()) {
            throw std::invalid_argument("Invalid or missing '"s + cdbi::ID.data() + "' field, it must be an integer");
        }
        id = moduleData[cdbi::ID];


        if (!moduleData.contains(cdbi::LOGIC_ADDRESS) ||
            !moduleData[cdbi::LOGIC_ADDRESS].is_number_integer()) {
            throw std::invalid_argument(
                "Invalid or missing '"s + cdbi::LOGIC_ADDRESS.data() + "' field, it must be an integer");
        }
        logicAddress = moduleData[cdbi::LOGIC_ADDRESS];

        if (!moduleData.contains(cdbi::NAME) ||
            !moduleData[cdbi::NAME].is_string()) {
            throw std::invalid_argument("Invalid or missing '"s + cdbi::NAME.data() + "' field, it must be a string");
        }
        name = moduleData[cdbi::NAME];

        if (!moduleData.contains(cdbi::CONFIG) ||
            !moduleData[cdbi::CONFIG].is_object()) {
            throw std::invalid_argument(
                "Invalid or missing '"s + cdbi::CONFIG.data() + "' field, it must be an object");
        }
        config = moduleData[cdbi::CONFIG];

        // LAST_ONLINE field is optional, return early if missing or null
        if (!moduleData.contains(cdbi::LAST_ONLINE) || moduleData[cdbi::LAST_ONLINE].is_null()) return;

        if (!moduleData[cdbi::LAST_ONLINE].is_string()) {
            throw std::invalid_argument("Invalid '"s + cdbi::LAST_ONLINE.data() + "' field, it must be a string");
        }
        lastOnline = Utils::parseTimestampTz(moduleData[cdbi::LAST_ONLINE]);
    }

    bool CachedModule::isFresh() const {
        return !stale;
    }

    nlohmann::json CachedModule::to_json() const {
        nlohmann::json json;
        json[cdbi::ID] = id;
        json[cdbi::LOGIC_ADDRESS] = logicAddress;
        json[cdbi::NAME] = name;
        json[cdbi::CONFIG] = config;
        if (lastOnline.has_value()) {
            json[cdbi::LAST_ONLINE] = Utils::timePointToTimestampTz(lastOnline.value());
        } else {
            json[cdbi::LAST_ONLINE] = nullptr;
        }
        return json;
    }

    CachedDevice::CachedDevice(const uint id,
                               const uint logicId,
                               const uint moduleId,
                               std::string name,
                               const std::string &type,
                               nlohmann::json config)
        : id(id), logicId(logicId), moduleId(moduleId), name(std::move(name)), type(type), config(std::move(config)) {
        verifyTypeValue(type); // Throws if invalid
    }

    CachedDevice::CachedDevice(const nlohmann::json &deviceData) {
        if (!deviceData.contains(cdbi::ID) ||
            !deviceData[cdbi::ID].is_number_integer()) {
            throw std::invalid_argument("Invalid or missing '"s + cdbi::ID.data() + "' field, it must be an integer");
        }
        id = deviceData[cdbi::ID];


        if (!deviceData.contains(cdbi::LOGIC_ID) ||
            !deviceData[cdbi::LOGIC_ID].is_number_integer()) {
            throw std::invalid_argument(
                "Invalid or missing '"s + cdbi::LOGIC_ID.data() + "' field, it must be an integer");
        }
        logicId = deviceData[cdbi::LOGIC_ID];

        if (!deviceData.contains(cdbi::MODULE_ID) ||
            !deviceData[cdbi::MODULE_ID].is_number_integer()) {
            throw std::invalid_argument(
                "Invalid or missing '"s + cdbi::MODULE_ID.data() + "' field, it must be an integer");
        }
        moduleId = deviceData[cdbi::MODULE_ID];

        if (!deviceData.contains(cdbi::NAME) ||
            !deviceData[cdbi::NAME].is_string()) {
            throw std::invalid_argument("Invalid or missing '"s + cdbi::NAME.data() + "' field, it must be a string");
        }
        name = deviceData[cdbi::NAME];

        if (!deviceData.contains(cdbi::TYPE) ||
            !deviceData[cdbi::TYPE].is_string()) {
            throw std::invalid_argument("Invalid or missing '"s + cdbi::TYPE.data() + "' field, it must be a string");
        }
        verifyTypeValue(deviceData[cdbi::TYPE].get<std::string_view>()); // Throws if invalid
        type = deviceData[cdbi::TYPE];

        if (!deviceData.contains(cdbi::CONFIG) ||
            !deviceData[cdbi::CONFIG].is_object()) {
            throw std::invalid_argument(
                "Invalid or missing '"s + cdbi::CONFIG.data() + "' field, it must be an object");
        }
        config = deviceData[cdbi::CONFIG];
    }

    bool CachedDevice::useCache() const {
        if (config.contains(cdck::USE_CACHE) && config[cdck::USE_CACHE].is_boolean()) {
            return config[cdck::USE_CACHE].get<bool>();
        }
        return true; // Default to using cache if not specified
    }

    std::chrono::seconds CachedDevice::cacheTTL() const {
        if (config.contains(cdck::CACHE_TTL) && config[cdck::CACHE_TTL].is_number_unsigned()) {
            return std::chrono::seconds(config[cdck::CACHE_TTL].get<uint64_t>());
        }
        return msDEFAULT_TTL;
    }

    bool CachedDevice::isFresh() const {
        return !stale;
    }

    void CachedDevice::verifyTypeValue(const std::string_view type) {
        if (Constants::DeviceTypes::TYPES.contains(type)) return; // Return on valid

        std::string validTypes;
        bool isFirst = true;
        for (const auto &validType: Constants::DeviceTypes::TYPES) {
            if (!isFirst) validTypes += ", ";
            validTypes += validType.data();
            isFirst = false;
        }

        throw std::invalid_argument(
            "Invalid '"s + cdbi::TYPE.data() + "' field value, valid values: " + validTypes);
    }

    nlohmann::json CachedDevice::to_json() const {
        nlohmann::json json;
        json[cdbi::ID] = id;
        json[cdbi::LOGIC_ID] = logicId;
        json[cdbi::MODULE_ID] = moduleId;
        json[cdbi::NAME] = name;
        json[cdbi::TYPE] = type;
        json[cdbi::CONFIG] = config;
        return json;
    }

    CachedReading::CachedReading(const uint deviceId,
                                 nlohmann::json value,
                                 const std::chrono::system_clock::time_point timestamp,
                                 nlohmann::json metadata)
        : deviceId(deviceId), value(std::move(value)), timestamp(timestamp), metadata(std::move(metadata)) {
    }

    nlohmann::json CachedReading::to_json() const {
        nlohmann::json json;
        json[cdbi::DEVICE_ID] = deviceId;
        json[cc::VALUE] = value;
        json[cdbi::TIMESTAMP] = Utils::timePointToTimestampTz(timestamp);
        json[cdbi::METADATA] = metadata;
        json[cc::STALE] = stale;
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

        if (expected != module.isFresh()) return false;

        // Inverses isFresh and stale logic - desired isFresh = false means module is stale
        module.stale = !desired;

        return true;
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
        std::shared_lock lock(mMutex);

        const auto devicesIds = getDeviceIdsForModuleUnlocked(moduleId);

        std::vector<CachedDevice> devices;
        devices.reserve(devicesIds.size());
        for (const auto deviceId: devicesIds) {
            const auto deviceOpt = getDeviceUnlocked(deviceId);
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

        const auto deviceIds = getDeviceIdsForModuleUnlocked(moduleId);
        for (const auto id: deviceIds) {
            eraseDeviceUnlocked(id);
        }

        removeFromModulesIndex(iter->second.logicAddress);
        mModules.erase(iter);
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
        return getDeviceUnlocked(deviceId, isFresh);
    }

    std::optional<bool> ConfigCache::compareExchangeIsDeviceFresh(const uint deviceId,
                                                                  const bool expected,
                                                                  const bool desired) {
        std::unique_lock lock(mMutex);

        const auto iter = mDevices.find(deviceId);
        if (iter == mDevices.end()) return std::nullopt;
        auto &device = iter->second;

        if (expected != device.isFresh()) return false;

        // Inverses isFresh and stale logic - desired (isFresh) = false means module is stale
        device.stale = !desired;

        return true;
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
        eraseDeviceUnlocked(deviceId);
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
        return getDeviceIdsForModuleUnlocked(moduleId);
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

    std::vector<uint> ConfigCache::getDeviceIdsForModuleUnlocked(const uint moduleId) const {
        std::vector<uint> deviceIds;

        for (const auto &[key, deviceId]: mDevicesIndex) {
            if (key.first == moduleId) {
                deviceIds.push_back(deviceId);
            }
        }
        return deviceIds;
    }

    void ConfigCache::eraseDeviceUnlocked(const uint deviceId) {
        const auto iter = mDevices.find(deviceId);
        if (iter == mDevices.end()) return;

        removeFromDevicesIndex(iter->second.moduleId, iter->second.logicId);
        mDevices.erase(iter);
    }

    std::optional<CachedDevice> ConfigCache::getDeviceUnlocked(const uint deviceId, const bool isFresh) const {
        const auto iter = mDevices.find(deviceId);
        if (iter == mDevices.end()) return std::nullopt;
        auto &device = iter->second;

        if (isFresh && !device.isFresh()) return std::nullopt;
        return device;
    }

    ReadingsCache::ReadingsCache(const ConfigCache &configCache, Time::ITimeProvider &timeProvider)
        : mConfigCache(configCache), mTimeProvider(timeProvider) {
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
        auto reading = iter->second;
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
        auto reading = CachedReading(deviceId, value, mTimeProvider.now(), metadata);
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

    bool ReadingsCache::isFresh(const CachedReading &reading, const std::chrono::seconds ttl) const {
        const auto age = mTimeProvider.now() - reading.timestamp;
        return age < ttl;
    }
}
