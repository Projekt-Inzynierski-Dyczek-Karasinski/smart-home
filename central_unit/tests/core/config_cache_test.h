#pragma once
#include "cache.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    inline void PrintTo(const CachedModule &m, std::ostream *os) {
        *os << m.to_json().dump();
    }

    inline void PrintTo(const CachedDevice &m, std::ostream *os) {
        *os << m.to_json().dump();
    }

    class ConfigCacheTest : public ::testing::Test {
    protected:
        // Helpers

        static nlohmann::json validModuleConfigJson(bool withLastOnline = false);

        static nlohmann::json validDeviceConfigJson();

        std::vector<CachedModule> populateConfigCacheWithModules(uint numberOfModules);

        std::vector<CachedDevice> populateConfigCacheWithDevices(uint numberOfDevices, uint moduleId = 1);

        ConfigCache mConfigCache;
    };
}
