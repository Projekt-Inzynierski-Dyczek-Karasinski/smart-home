#include "config_cache_test.h"
#include "utils.h"

#include <gmock/gmock.h>

namespace SmartHome {
    //region helpers
    nlohmann::json ConfigCacheTest::validModuleConfigJson(const bool withLastOnline) {
        nlohmann::json json = {
            {"id", 1},
            {"logic_address", 11},
            {"name", "testModule"},
            {"config", nlohmann::json::object()},
        };

        if (withLastOnline) {
            json["last_online"] = "2026-07-15 18:15:02+02";
        }
        return json;
    }

    nlohmann::json ConfigCacheTest::validDeviceConfigJson() {
        return {
            {"id", 1},
            {"logic_id", 11},
            {"module_id", 2},
            {"name", "testModule"},
            {"type", "sensor"},
            {
                "config", {
                    {"use_cache", true},
                    {"cache_ttl", 300u}
                }
            },
        };
    }

    std::vector<CachedModule> ConfigCacheTest::populateConfigCacheWithModules(const uint numberOfModules) {
        std::vector<CachedModule> result;
        result.reserve(numberOfModules);

        auto cachedModule = CachedModule(validModuleConfigJson());

        for (uint i = 0; i < numberOfModules; i++) {
            cachedModule.id = i + 1;
            cachedModule.logicAddress = cachedModule.id + 10;
            mConfigCache.setModule(cachedModule);
            result.push_back(cachedModule);
        }

        return result;
    }

    std::vector<CachedDevice> ConfigCacheTest::populateConfigCacheWithDevices(const uint numberOfDevices,
                                                                              const uint moduleId) {
        std::vector<CachedDevice> result;
        result.reserve(numberOfDevices);

        auto cachedDevice = CachedDevice(validDeviceConfigJson());

        for (uint i = 0; i < numberOfDevices; i++) {
            cachedDevice.id = moduleId * 10 + i + 1;
            cachedDevice.logicId = i + 1;
            cachedDevice.moduleId = moduleId;

            mConfigCache.setDevice(cachedDevice);
            result.push_back(cachedDevice);
        }

        return result;
    }


    //endregion

    //region CachedModule
    TEST_F(ConfigCacheTest, CachedModuleValidFormat) {
        auto moduleData = validModuleConfigJson(true);

        // Tests with last online
        EXPECT_NO_THROW(CachedModule{moduleData});
        EXPECT_NO_THROW(CachedModule(moduleData["id"],
            moduleData["logic_address"],
            moduleData["name"],
            moduleData["config"],
            SmartHome::Utils::parseTimestampTz(moduleData["last_online"])));


        // Test with null last online
        moduleData["last_online"] = nullptr;
        EXPECT_NO_THROW(CachedModule{moduleData});

        // Tests with no last online
        moduleData.erase("last_online");
        EXPECT_NO_THROW(CachedModule{moduleData});
        EXPECT_NO_THROW(CachedModule(moduleData["id"],
            moduleData["logic_address"],
            moduleData["name"],
            moduleData["config"]));
    }

    TEST_F(ConfigCacheTest, CachedModuleMissingOrWrongId) {
        auto moduleData = validModuleConfigJson();

        moduleData.erase("id");
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("id")));

        moduleData["id"] = "1";
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("id")));
    }

    TEST_F(ConfigCacheTest, CachedModuleMissingOrWrongLogicAddress) {
        auto moduleData = validModuleConfigJson();

        moduleData.erase("logic_address");
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("logic_address")));

        moduleData["logic_address"] = "1";
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("logic_address")));
    }

    TEST_F(ConfigCacheTest, CachedModuleMissingOrWrongName) {
        auto moduleData = validModuleConfigJson();

        moduleData.erase("name");
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("name")));

        moduleData["name"] = 1;
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("name")));
    }

    TEST_F(ConfigCacheTest, CachedModuleMissingOrWrongConfig) {
        auto moduleData = validModuleConfigJson();

        moduleData.erase("config");
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("config")));

        moduleData["config"] = nlohmann::json::array();
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("config")));
    }

    TEST_F(ConfigCacheTest, CachedModuleWrongLastOnline) {
        auto moduleData = validModuleConfigJson(true);

        moduleData["last_online"] = 123456789;
        EXPECT_THAT([&] { CachedModule{moduleData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("last_online")));
    }

    TEST_F(ConfigCacheTest, CachedModuleToJSON) {
        auto jsonRaw = validModuleConfigJson(true);
        auto jsonParsed = CachedModule(jsonRaw).to_json();

        auto lastOnlineRawStr = jsonRaw["last_online"].get<std::string>();
        jsonRaw.erase("last_online");

        auto lastOnlineParsedStr = jsonParsed["last_online"].get<std::string>();
        jsonParsed.erase("last_online");

        EXPECT_EQ(jsonRaw, jsonParsed); // Compare without last_online

        // Parse lastOnlineRawStr before comparing
        const auto timePoint = Utils::parseTimestampTz(lastOnlineRawStr);
        lastOnlineRawStr = Utils::timePointToTimestampTz(timePoint);

        EXPECT_EQ(lastOnlineRawStr, lastOnlineParsedStr);
    }

    TEST_F(ConfigCacheTest, CachedModuleToJSONNullLastOnline) {
        const auto json = CachedModule(validModuleConfigJson()).to_json();

        ASSERT_TRUE(json.contains("last_online"));
        EXPECT_TRUE(json["last_online"].is_null());
    }

    //endregion

    //region CachedDevice
    TEST_F(ConfigCacheTest, CachedDeviceValidFormat) {
        auto deviceData = validDeviceConfigJson();

        EXPECT_NO_THROW(CachedDevice{deviceData});
        EXPECT_NO_THROW(CachedDevice(deviceData["id"],
            deviceData["logic_id"],
            deviceData["module_id"],
            deviceData["name"],
            deviceData["type"],
            deviceData["config"]));
    }

    TEST_F(ConfigCacheTest, CachedDeviceMissingOrWrongId) {
        auto deviceData = validDeviceConfigJson();

        deviceData.erase("id");
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("id")));

        deviceData["id"] = "1";
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("id")));
    }

    TEST_F(ConfigCacheTest, CachedDeviceMissingOrWrongLogicId) {
        auto deviceData = validDeviceConfigJson();

        deviceData.erase("logic_id");
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("logic_id")));

        deviceData["logic_id"] = "1";
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("logic_id")));
    }

    TEST_F(ConfigCacheTest, CachedDeviceMissingOrWrongModuleId) {
        auto deviceData = validDeviceConfigJson();

        deviceData.erase("module_id");
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("module_id")));

        deviceData["module_id"] = "1";
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("module_id")));
    }

    TEST_F(ConfigCacheTest, CachedDeviceMissingOrWrongName) {
        auto deviceData = validDeviceConfigJson();

        deviceData.erase("name");
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("name")));

        deviceData["name"] = 1;
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("name")));
    }

    TEST_F(ConfigCacheTest, CachedDeviceMissingOrWrongType) {
        auto deviceData = validDeviceConfigJson();

        deviceData.erase("type");
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("missing 'type'")));

        deviceData["type"] = 1;
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("Invalid or missing 'type'")));

        deviceData["type"] = "invalid_type";
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("Invalid 'type'")));

        EXPECT_THAT([&] { CachedDevice(deviceData["id"],
                        deviceData["logic_id"],
                        deviceData["module_id"],
                        deviceData["name"],
                        deviceData["type"],
                        deviceData["config"]); },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("Invalid 'type'")));
    }

    TEST_F(ConfigCacheTest, CachedDeviceMissingOrWrongConfig) {
        auto deviceData = validDeviceConfigJson();

        deviceData.erase("config");
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("config")));

        deviceData["config"] = nlohmann::json::array();
        EXPECT_THAT([&] { CachedDevice{deviceData}; },
                    ::testing::ThrowsMessage<std::invalid_argument>(::testing::HasSubstr("config")));
    }

    TEST_F(ConfigCacheTest, CachedDeviceUseCache) {
        auto cachedDevice = CachedDevice(validDeviceConfigJson());

        EXPECT_TRUE(cachedDevice.useCache());

        cachedDevice.config["use_cache"] = false;
        EXPECT_FALSE(cachedDevice.useCache());

        cachedDevice.config.erase("use_cache");
        EXPECT_TRUE(cachedDevice.useCache());
    }

    TEST_F(ConfigCacheTest, CachedDeviceCacheTTL) {
        auto cachedDevice = CachedDevice(validDeviceConfigJson());

        EXPECT_EQ(cachedDevice.cacheTTL().count(), 300);

        cachedDevice.config.erase("cache_ttl");
        EXPECT_EQ(cachedDevice.cacheTTL().count(), 60);
    }

    TEST_F(ConfigCacheTest, CachedDeviceToJSON) {
        const auto json = CachedDevice(validDeviceConfigJson()).to_json();
        EXPECT_EQ(validDeviceConfigJson(), json);
    }

    //endregion

    //region ConfigCache

    TEST_F(ConfigCacheTest, ConfigCacheSetAndGetModule) {
        const auto modules = populateConfigCacheWithModules(2);

        EXPECT_EQ(mConfigCache.modulesSize(), 2);

        EXPECT_TRUE(mConfigCache.getModule(modules[0].id).has_value());
        EXPECT_TRUE(mConfigCache.getModule(modules[1].id).has_value());

        EXPECT_EQ(modules[0], mConfigCache.getModule(modules[0].id).value());
        EXPECT_EQ(modules[1], mConfigCache.getModule(modules[1].id).value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetMissingModule) {
        EXPECT_EQ(mConfigCache.getModule(1), std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetFreshModule) {
        const auto modules = populateConfigCacheWithModules(2);

        ASSERT_THAT(mConfigCache.compareExchangeIsModuleFresh(2, true, false), ::testing::Optional(true));

        EXPECT_TRUE(mConfigCache.getModule(1, true).has_value());
        EXPECT_FALSE(mConfigCache.getModule(2, true).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheCompareExchangeIsModuleFresh) {
        populateConfigCacheWithModules(1);
        constexpr uint moduleId = 1;

        EXPECT_TRUE(mConfigCache.getModule(moduleId, true).has_value());
        EXPECT_THAT(mConfigCache.compareExchangeIsModuleFresh(moduleId, true, false), ::testing::Optional(true));

        // Successfully set as stale
        EXPECT_FALSE(mConfigCache.getModule(moduleId, true).has_value());
        EXPECT_THAT(mConfigCache.compareExchangeIsModuleFresh(moduleId, true, true), ::testing::Optional(false));

        // Failed to set as fresh, expected value not matched
        EXPECT_FALSE(mConfigCache.getModule(moduleId, true).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheCompareExchangeIsModuleFreshMissingModule) {
        EXPECT_EQ(mConfigCache.compareExchangeIsModuleFresh( 1, true, false), std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetAllModules) {
        const auto modules = populateConfigCacheWithModules(2);
        EXPECT_THAT(mConfigCache.getAllModules(), ::testing::UnorderedElementsAreArray(modules));
    }

    TEST_F(ConfigCacheTest, ConfigCacheEraseModule) {
        populateConfigCacheWithModules(2);
        populateConfigCacheWithDevices(1, 1);
        constexpr uint moduleId = 1;

        EXPECT_TRUE(mConfigCache.getModule(moduleId).has_value());
        EXPECT_EQ(mConfigCache.getModuleDevices(moduleId).size(), 1);

        mConfigCache.eraseModule(moduleId);

        EXPECT_EQ(mConfigCache.modulesSize(), 1);
        EXPECT_FALSE(mConfigCache.getModule(moduleId).has_value());
        EXPECT_THAT(mConfigCache.getModuleDevices(moduleId), ::testing::IsEmpty());
    }

    TEST_F(ConfigCacheTest, ConfigCacheUpdateModuleLastOnline) {
        populateConfigCacheWithModules(1);
        constexpr uint moduleId = 1;

        const auto timestamp = std::chrono::system_clock::now();
        mConfigCache.updateModuleLastOnline(moduleId, timestamp);

        EXPECT_EQ(mConfigCache.getModule(moduleId).value().lastOnline, timestamp);
    }

    TEST_F(ConfigCacheTest, ConfigCacheUpdateModule) {
        const auto modules = populateConfigCacheWithModules(1);

        EXPECT_EQ(mConfigCache.modulesSize(), 1);

        auto updatedCachedModule = modules[0];
        updatedCachedModule.logicAddress += 1;
        mConfigCache.setModule(updatedCachedModule);

        EXPECT_EQ(mConfigCache.modulesSize(), 1);
        EXPECT_EQ(mConfigCache.findModuleId(modules[0].logicAddress), std::nullopt);
        EXPECT_EQ(mConfigCache.getModule(modules[0].id).value().logicAddress, modules[0].logicAddress + 1);
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetModuleDevices) {
        const auto modules = populateConfigCacheWithModules(3);
        const auto module1Devices = populateConfigCacheWithDevices(2, 1);
        const auto module3Devices = populateConfigCacheWithDevices(4, 3);

        EXPECT_THAT(mConfigCache.getModuleDevices(1), ::testing::UnorderedElementsAreArray(module1Devices));
        EXPECT_THAT(mConfigCache.getModuleDevices(3), ::testing::UnorderedElementsAreArray(module3Devices));

        EXPECT_THAT(mConfigCache.getModuleDevices(2), ::testing::IsEmpty());
        EXPECT_THAT(mConfigCache.getModuleDevices(4), ::testing::IsEmpty());
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetDeviceIdsForModule) {
        const auto modules = populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(2);

        EXPECT_THAT(mConfigCache.getDeviceIdsForModule(1),
                    ::testing::UnorderedElementsAre(devices[0].id, devices[1].id));
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindModuleId) {
        const auto modules = populateConfigCacheWithModules(2);

        EXPECT_EQ(mConfigCache.findModuleId(modules[1].logicAddress).value(), modules[1].id);
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindModuleIdMissingModule) {
        const auto modules = populateConfigCacheWithModules(1);

        EXPECT_FALSE(mConfigCache.findModuleId(modules[0].logicAddress+1).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheSetAndGetDevice) {
        populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(2, 1);

        EXPECT_EQ(mConfigCache.devicesSize(), 2);

        EXPECT_TRUE(mConfigCache.getDevice(devices[0].id).has_value());
        EXPECT_TRUE(mConfigCache.getDevice(devices[1].id).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheCompareExchangeIsDeviceFresh) {
        populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(2, 1);
        const auto deviceId = devices[0].id;

        EXPECT_TRUE(mConfigCache.getDevice(deviceId, true).has_value());
        EXPECT_THAT(mConfigCache.compareExchangeIsDeviceFresh(deviceId, true, false), ::testing::Optional(true));

        // Successfully set as stale
        EXPECT_FALSE(mConfigCache.getDevice(deviceId, true).has_value());
        EXPECT_THAT(mConfigCache.compareExchangeIsDeviceFresh(deviceId, true, true), ::testing::Optional(false));

        // Failed to set as fresh, expected value not matched
        EXPECT_FALSE(mConfigCache.getDevice(deviceId, true).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetMissingDevice) {
        EXPECT_EQ(mConfigCache.getDevice(1), std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheCompareExchangeIsDeviceFreshMissingDevice) {
        EXPECT_EQ(mConfigCache.compareExchangeIsDeviceFresh(1, true, false), std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheGetAllDevices) {
        populateConfigCacheWithModules(3);
        const auto devices1 = populateConfigCacheWithDevices(6, 1);
        const auto devices2 = populateConfigCacheWithDevices(4, 2);
        const auto devices3 = populateConfigCacheWithDevices(8, 3);

        std::vector<CachedDevice> allDevices;
        allDevices.insert(allDevices.end(), devices1.begin(), devices1.end());
        allDevices.insert(allDevices.end(), devices2.begin(), devices2.end());
        allDevices.insert(allDevices.end(), devices3.begin(), devices3.end());

        EXPECT_EQ(mConfigCache.devicesSize(), 18);

        EXPECT_THAT(allDevices, ::testing::UnorderedElementsAreArray(mConfigCache.getAllDevices()));
    }

    TEST_F(ConfigCacheTest, ConfigCacheEraseDevice) {
        populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(2, 1);

        EXPECT_EQ(mConfigCache.devicesSize(), 2);
        EXPECT_TRUE(mConfigCache.getDevice(devices[0].id).has_value());

        mConfigCache.eraseDevice(devices[0].id);

        EXPECT_EQ(mConfigCache.devicesSize(), 1);
        EXPECT_FALSE(mConfigCache.getDevice(devices[0].id).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheUpdateDevice) {
        const auto modules = populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(2);

        EXPECT_EQ(mConfigCache.devicesSize(), 2);

        auto updatedCachedDevice = devices[0];
        updatedCachedDevice.logicId += 1;
        mConfigCache.setDevice(updatedCachedDevice);

        EXPECT_EQ(mConfigCache.devicesSize(), 2);
        EXPECT_EQ(mConfigCache.findDeviceId(1, devices[0].logicId), std::nullopt);
        EXPECT_EQ(mConfigCache.getDevice(devices[0].id).value().logicId, devices[0].logicId + 1);
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindDeviceId) {
        const auto modules = populateConfigCacheWithModules(3);
        const auto devices = populateConfigCacheWithDevices(3, 2);

        EXPECT_THAT(mConfigCache.findDeviceId(modules[1].id, devices[1].logicId), ::testing::Optional(devices[1].id));
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindDeviceIdByLogicAddress) {
        const auto modules = populateConfigCacheWithModules(3);
        const auto devices = populateConfigCacheWithDevices(3, 2);

        EXPECT_THAT(mConfigCache.findDeviceIdByLogicAddress(modules[1].logicAddress, devices[1].logicId),
                    ::testing::Optional(devices[1].id));
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindDeviceIdMissingDevice) {
        const auto modules = populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(1);

        EXPECT_EQ(mConfigCache.findDeviceId(modules[0].id, devices[0].logicId + 1), std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindDeviceIdByLogicAddressMissingModule) {
        const auto modules = populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(1);

        EXPECT_EQ(mConfigCache.findDeviceIdByLogicAddress(modules[0].logicAddress + 1, devices[0].logicId),
                  std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheFindDeviceIdByLogicAddressMissingDevice) {
        const auto modules = populateConfigCacheWithModules(1);
        const auto devices = populateConfigCacheWithDevices(1);

        EXPECT_EQ(mConfigCache.findDeviceIdByLogicAddress(modules[0].logicAddress, devices[0].logicId + 1),
                  std::nullopt);
    }

    TEST_F(ConfigCacheTest, ConfigCacheClearModules) {
        const auto modules = populateConfigCacheWithModules(2);
        const auto devices = populateConfigCacheWithDevices(2);

        mConfigCache.clearModules();

        EXPECT_EQ(mConfigCache.modulesSize(), 0);
        EXPECT_EQ(mConfigCache.findModuleId(modules[0].logicAddress), std::nullopt);

        // Devices remain untouched
        EXPECT_EQ(mConfigCache.devicesSize(), 2);
        EXPECT_TRUE(mConfigCache.getDevice(devices[0].id).has_value());
    }

    TEST_F(ConfigCacheTest, ConfigCacheClearDevices) {
        const auto modules = populateConfigCacheWithModules(2);
        const auto devices = populateConfigCacheWithDevices(2);

        mConfigCache.clearDevices();

        EXPECT_EQ(mConfigCache.devicesSize(), 0);
        EXPECT_EQ(mConfigCache.findDeviceId(devices[0].moduleId, devices[0].logicId), std::nullopt);

        // Modules remain untouched
        EXPECT_EQ(mConfigCache.modulesSize(), 2);
        EXPECT_TRUE(mConfigCache.getModule(modules[0].id).has_value());
    }

    //endregion
}
