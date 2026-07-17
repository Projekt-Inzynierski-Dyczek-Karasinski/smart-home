#pragma once
#include "cache.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    inline void PrintTo(const CachedReading &m, std::ostream *os) {
        *os << m.to_json().dump();
    }

    class ReadingsCacheTest : public ::testing::Test {
    protected:
        void populateDevices(uint id = 1, bool useCache = true, uint cacheTTL = 60);

        void populateReadings(uint id = 1, const nlohmann::json &value = 12.34, const nlohmann::json &metadata = {});

        // Controllable time source injected into ReadingsCache via the Clock lambda
        std::chrono::system_clock::time_point mNow = std::chrono::system_clock::now();

        ConfigCache mConfigCache;
        ReadingsCache mReadingsCache{mConfigCache, [this] { return mNow; }};
    };
}
