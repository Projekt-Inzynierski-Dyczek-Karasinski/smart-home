#include "readings_cache_test.h"
#include "utils.h"

namespace SmartHome::Tests {
    void ReadingsCacheTest::populateDevices(const uint id, bool useCache, uint cacheTTL) {
        const auto device = CachedDevice(id, id, 1, "test", "sensor", {
                                             {"use_cache", useCache},
                                             {"cache_ttl", cacheTTL}
                                         });

        mConfigCache.setDevice(device);
    }

    void ReadingsCacheTest::populateReadings(const uint id, const nlohmann::json &value,
                                             const nlohmann::json &metadata) {
        mReadingsCache.set(id, value, metadata);
    }

    TEST_F(ReadingsCacheTest, CachedReadingToJson) {
        constexpr uint id = 1;
        const nlohmann::json value = 12.34;
        const auto timestamp = std::chrono::system_clock::now();
        const nlohmann::json metadata = {{"type", "sensor_value"}};

        const auto reading = CachedReading(id, value, timestamp, metadata);

        const auto readingJson = reading.to_json();

        EXPECT_EQ(readingJson["device_id"], id);
        EXPECT_EQ(readingJson["value"], value);
        EXPECT_EQ(readingJson["timestamp"], Utils::timePointToTimestampTz(timestamp));
        EXPECT_EQ(readingJson["metadata"], metadata);
        EXPECT_FALSE(readingJson["stale"]);
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheSetAndGetReading) {
        populateDevices();
        populateReadings(1, 12.34, {{"type", "sensor_value"}});

        const auto cachedReading = mReadingsCache.get(1);

        ASSERT_TRUE(cachedReading.has_value());

        EXPECT_EQ(cachedReading.value().value, 12.34);
        EXPECT_EQ(cachedReading.value().metadata, nlohmann::json({{"type", "sensor_value"}}));
        EXPECT_FALSE(cachedReading.value().stale);
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetMissingReading) {
        populateReadings(1);

        ASSERT_EQ(mReadingsCache.size(), 1);
        const auto cachedReading = mReadingsCache.get(2);

        EXPECT_FALSE(cachedReading.has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheEraseReading) {
        populateReadings();
        EXPECT_TRUE( mReadingsCache.get(1).has_value());

        mReadingsCache.erase(1);
        EXPECT_FALSE(mReadingsCache.get(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheClearReadings) {
        populateReadings(1);
        populateReadings(2);
        populateReadings(3);

        EXPECT_EQ(mReadingsCache.size(), 3);

        mReadingsCache.clear();
        EXPECT_EQ(mReadingsCache.size(), 0);
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheOverrideReading) {
        populateReadings(1, 12.34);
        EXPECT_EQ(mReadingsCache.get(1).value().value, 12.34);

        populateReadings(1, 0);
        EXPECT_EQ(mReadingsCache.get(1).value().value, 0);
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetFreshReading) {
        populateDevices();
        populateReadings();

        EXPECT_TRUE(mReadingsCache.getFresh(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetFreshMissingReading) {
        populateDevices();

        EXPECT_FALSE(mReadingsCache.getFresh(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetFreshReadingStaleNoCache) {
        populateDevices(1,false);
        populateReadings();

        EXPECT_FALSE(mReadingsCache.getFresh(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetFreshReadingStaleZeroTime) {
        populateDevices(1,true, 0);
        populateReadings();

        EXPECT_FALSE(mReadingsCache.getFresh(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetFreshReadingTurnedStale) {
        populateDevices(1,true);
        populateReadings();

        EXPECT_TRUE(mReadingsCache.getFresh(1).has_value());

        mTimeProvider.advanceBy(59s);

        EXPECT_TRUE(mReadingsCache.getFresh(1).has_value());

        mTimeProvider.advanceBy(2s);

        EXPECT_FALSE(mReadingsCache.getFresh(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetStaleReading) {
        populateDevices(1, false);
        populateReadings(1, 12.34);

        const auto cachedReading = mReadingsCache.get(1);

        ASSERT_TRUE(cachedReading.has_value());
        EXPECT_TRUE(cachedReading.value().stale);
        EXPECT_EQ(cachedReading.value().value, 12.34);
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheGetFreshReadingUnknownDevice) {
        populateReadings(1); // No device in ConfigCache

        EXPECT_FALSE(mReadingsCache.getFresh(1).has_value());
    }

    TEST_F(ReadingsCacheTest, ReadingsCacheOverrideRefreshesTimestamp) {
        populateDevices(1, true);
        populateReadings(1);
        mTimeProvider.advanceBy(61s);
        ASSERT_FALSE(mReadingsCache.getFresh(1).has_value());

        populateReadings(1); // Overwrite resets timestamp
        EXPECT_TRUE(mReadingsCache.getFresh(1).has_value());
    }
}
