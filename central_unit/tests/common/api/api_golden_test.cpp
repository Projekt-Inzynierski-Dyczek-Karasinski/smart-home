#include "api_golden_test.h"

namespace SmartHome::Tests {
    nlohmann::json ApiGoldenTest::roundTrip(const API::ApiRequest &request) {
        return API::ApiRequest(nlohmann::json::parse(request.to_string())).to_json();
    }

    nlohmann::json ApiGoldenTest::roundTrip(const API::ApiResponse &response) {
        return API::ApiResponse(nlohmann::json::parse(response.to_string())).to_json();
    }


    //region Requests

    TEST_F(ApiGoldenTest, DbQueryRequestValidFormat) {
        const auto message = R"json({
            "id":79,
            "jsonrpc":"2.0",
            "method":"database.get",
            "params":{"columns":["id","name","logic_address","config","last_online"],"table":"modules"}
        })json"_json;

        const API::ApiRequest request(message);

        EXPECT_EQ(request.method, "database.get");
        ASSERT_TRUE(request.id.hasValue());
        EXPECT_EQ(request.id.value(), apiId_t{79});
        ASSERT_TRUE(request.params.has_value());
        EXPECT_EQ(*request.params, message.at("params"));
        EXPECT_EQ(request.to_json(), message);
        EXPECT_EQ(roundTrip(request), message);
    }

    TEST_F(ApiGoldenTest, GetRequestWithPositionalArguments) {
        const auto message = R"json({
            "id":33,
            "jsonrpc":"2.0",
            "method":"core.get",
            "params":{"args":[2,1],"module_id":1,"type":"device_readings"}
        })json"_json;

        const API::ApiRequest request(message);

        EXPECT_EQ(request.method, "core.get");
        ASSERT_TRUE(request.id.hasValue());
        EXPECT_EQ(request.id.value(), apiId_t{33});
        ASSERT_TRUE(request.params.has_value());
        EXPECT_EQ(request.params->at("args"), R"json([2,1])json"_json);
        EXPECT_EQ(request.to_json(), message);
        EXPECT_EQ(roundTrip(request), message);
    }

    TEST_F(ApiGoldenTest, NotificationValidFormat) {
        const auto message = R"json({
            "jsonrpc":"2.0",
            "method":"core.notify",
            "params":{"data":{"operation":"UPDATE"},"type":"modules_changed"}
        })json"_json;

        const API::ApiRequest request(message);

        EXPECT_EQ(request.method, "core.notify");
        EXPECT_TRUE(request.id.isUndefined());
        EXPECT_FALSE(request.to_json().contains("id"));
        EXPECT_EQ(request.to_json(), message);
        EXPECT_EQ(roundTrip(request), message);
    }

    //endregion

    //region Responses

    TEST_F(ApiGoldenTest, DbQueryResponseValidFormat) {
        const auto message = R"json({
            "id": 79,
            "jsonrpc": "2.0",
            "result": {
                "affected_rows": 2,
                "rows": [
                    {
                        "config": {
                            "connection": {"rf_channel": 2, "type": "radio"},
                            "default_sleep_duration": 3600000,
                            "on_notification": {
                                "alert": [
                                    {
                                        "action": {
                                            "method": "core.set",
                                            "params": {
                                                "mode": "overwrite",
                                                "module_id": 1,
                                                "path": "config.on_notification.alert.0.enabled",
                                                "type": "module",
                                                "value": false
                                            }
                                        },
                                        "enabled": false
                                    }
                                ]
                            },
                            "power_saving": true,
                            "sleep_after_send": true,
                            "type": "test"
                        },
                        "id": 1,
                        "last_online": "2026-09-21 20:30:01+02",
                        "logic_address": 2,
                        "name": "termometr"
                    },
                    {
                        "config": {
                            "connection": {"rf_channel": 3, "type": "radio"},
                            "default_sleep_duration": 3600000,
                            "power_saving": false,
                            "sleep_after_send": true,
                            "type": "test"
                        },
                        "id": 11,
                        "last_online": null,
                        "logic_address": 12,
                        "name": "test_module"
                    }
                ]
            }
        })json"_json;

        const API::ApiResponse response(message);

        ASSERT_TRUE(response.id.hasValue());
        EXPECT_EQ(response.id.value(), apiId_t{79});
        EXPECT_FALSE(response.error.has_value());
        ASSERT_TRUE(response.result.has_value());
        EXPECT_EQ(nlohmann::json::parse(*response.result), message.at("result"));
        EXPECT_EQ(response.to_json(), message);
        EXPECT_EQ(roundTrip(response), message);
    }

    TEST_F(ApiGoldenTest, DeviceReadingsResponsePreservesFloatingPointValues) {
        const auto message = R"json({
            "id":33,
            "jsonrpc":"2.0",
            "result":{
                "device_readings":[
                    {
                        "device_id":1,
                        "metadata":{"type":"sensor_value"},
                        "stale":true,
                        "timestamp":"2026-09-21T18:30:01Z",
                        "value":[51.599609375,984.2906494140625,24.829999923706055]
                    }
                ]
            }
        })json"_json;

        const API::ApiResponse response(message);

        ASSERT_TRUE(response.result.has_value());
        EXPECT_EQ(nlohmann::json::parse(*response.result), message.at("result"));
        EXPECT_EQ(response.to_json(), message);
        EXPECT_EQ(roundTrip(response), message);
    }

    TEST_F(ApiGoldenTest, ModuleDevicesResponsePreservesUtf8Labels) {
        const auto message = R"json({
            "id": 32,
            "jsonrpc": "2.0",
            "result": {
                "module_devices": [
                    {
                        "config": {
                            "cache_ttl": 0,
                            "events": [],
                            "schedule": [
                                {
                                    "action": {
                                        "method": "core.get",
                                        "params": {"args": [1], "module_id": 1, "type": "sensor_value"}
                                    },
                                    "dtstart": "2026-01-01T00:00:00Z",
                                    "enabled": true,
                                    "rrule": "FREQ=HOURLY;INTERVAL=1"
                                }
                            ],
                            "use_cache": false,
                            "values": [
                                {"index": 0, "label": "poziom naładowania", "unit": "%"}
                            ]
                        },
                        "id": 2,
                        "logic_id": 1,
                        "module_id": 1,
                        "name": "bateria",
                        "type": "sensor"
                    },
                    {
                        "config": {
                            "cache_ttl": 300,
                            "events": [
                                {
                                    "action": {
                                        "method": "core.set",
                                        "params": {
                                            "mode": "overwrite",
                                            "module_id": 1,
                                            "path": "config.on_notification.alert.0.enabled",
                                            "type": "module",
                                            "value": false
                                        }
                                    },
                                    "condition": {"$0": {">=": 50}},
                                    "enabled": false,
                                    "trigger": "level"
                                }
                            ],
                            "schedule": [
                                {
                                    "action": {
                                        "method": "core.get",
                                        "params": {"args": [2], "module_id": 1, "type": "device_value"}
                                    },
                                    "dtstart": "2026-01-01T00:00:00Z",
                                    "enabled": true,
                                    "rrule": "FREQ=MINUTELY;INTERVAL=30"
                                }
                            ],
                            "use_cache": true,
                            "values": [
                                {"index": 0, "label": "wilgotność", "unit": "%"},
                                {"index": 1, "label": "ciśnienie", "unit": "hPa"},
                                {"index": 2, "label": "temperatura", "precision": 2, "unit": "°C"}
                            ]
                        },
                        "id": 1,
                        "logic_id": 2,
                        "module_id": 1,
                        "name": "bme280",
                        "type": "sensor"
                    }
                ]
            }
        })json"_json;

        const API::ApiResponse response(message);

        ASSERT_TRUE(response.result.has_value());
        EXPECT_EQ(nlohmann::json::parse(*response.result), message.at("result"));
        EXPECT_EQ(response.to_json(), message);
        EXPECT_EQ(roundTrip(response), message);
    }

    // TODO change batch literal after fixing double sleep notification
    TEST_F(ApiGoldenTest, MediatorRequestBatchPreservesNotificationsAndIds) {
        const auto batch = R"json([
            {
                "jsonrpc": "2.0",
                "method": "module_mediator.execute",
                "params": {
                    "args": [1769999],
                    "module_info": {"logic_address": 2, "rf_channel": 2},
                    "type": "deep_sleep"
                }
            },
            {
                "id": 54,
                "jsonrpc": "2.0",
                "method": "module_mediator.get",
                "params": {
                    "args": [1],
                    "module_info": {"logic_address": 2, "rf_channel": 2},
                    "type": "sensor_value"
                }
            },
            {
                "jsonrpc": "2.0",
                "method": "module_mediator.execute",
                "params": {
                    "args": [1769998],
                    "module_info": {"logic_address": 2, "rf_channel": 2},
                    "type": "deep_sleep"
                }
            },
            {
                "id": 56,
                "jsonrpc": "2.0",
                "method": "module_mediator.get",
                "params": {
                    "args": [2],
                    "module_info": {"logic_address": 2, "rf_channel": 2},
                    "type": "sensor_value"
                }
            }
        ])json"_json;

        constexpr std::array<std::optional<apiId_t>, 4> expectedIds{std::nullopt, 54, std::nullopt, 56};

        ASSERT_EQ(batch.size(), expectedIds.size());

        for (size_t i = 0; i < batch.size(); ++i) {
            SCOPED_TRACE("batch index " + std::to_string(i));
            const API::ApiRequest request(batch[i]);

            if (expectedIds[i].has_value()) {
                ASSERT_TRUE(request.id.hasValue());
                EXPECT_EQ(request.id.value(), *expectedIds[i]);
            } else {
                EXPECT_TRUE(request.id.isUndefined());
            }
            EXPECT_EQ(request.to_json(), batch[i]);
            EXPECT_EQ(roundTrip(request), batch[i]);
        }
    }

    TEST_F(ApiGoldenTest, MediatorResponseBatchPreservesPrimitiveAndArrayResults) {
        const auto batch = R"json([
            {
                "id":54,
                "jsonrpc":"2.0",
                "result":100
            },
            {
                "id":56,
                "jsonrpc":"2.0",
                "result":[51.56640625,984.2255249023438,24.6299991607666]
            }
        ])json"_json;

        constexpr std::array<apiId_t, 2> expectedIds{54, 56};

        ASSERT_EQ(batch.size(), expectedIds.size());

        for (size_t i = 0; i < batch.size(); ++i) {
            SCOPED_TRACE("batch index " + std::to_string(i));
            const API::ApiResponse response(batch[i]);

            ASSERT_TRUE(response.id.hasValue());
            EXPECT_EQ(response.id.value(), expectedIds[i]);
            ASSERT_TRUE(response.result.has_value());
            EXPECT_EQ(nlohmann::json::parse(*response.result), batch[i].at("result"));
            EXPECT_EQ(response.to_json(), batch[i]);
            EXPECT_EQ(roundTrip(response), batch[i]);
        }
    }

    //endregion
}
