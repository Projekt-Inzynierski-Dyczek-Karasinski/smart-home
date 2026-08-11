#include "action_helpers_test.h"

namespace SmartHome::Tests {
    TEST_F(ActionHelpersTest, CommandMetadataLifeCycle) {
        const auto command = API::InternalApi::Command{
            {{"key", "value"}},
            API::ApiId(API::getNextApiId()),
            API::InternalApi::Method(API::InternalApi::MethodTypes::SET),
            API::InternalApi::Target(API::InternalApi::TargetTypes::CORE)
        };

        ActionHelpers::CommandMetadata cmdMeta{
            command,
            {},
            API::getNextApiId()
        };

        EXPECT_TRUE(cmdMeta.isPending());
        EXPECT_TRUE(cmdMeta.cancel());

        EXPECT_FALSE(cmdMeta.cancel());
        EXPECT_FALSE(cmdMeta.isPending());
    }

    TEST_F(ActionHelpersTest, CommandMetadataPropagatingIsNotification) {
        const auto command = API::InternalApi::Command{
            {{"key", "value"}},
            API::ApiId(nullptr),
            API::InternalApi::Method(API::InternalApi::MethodTypes::SET),
            API::InternalApi::Target(API::InternalApi::TargetTypes::CORE)
        };

        const ActionHelpers::CommandMetadata cmdMeta{
            command,
            {},
            API::getNextApiId()
        };

        EXPECT_TRUE(cmdMeta.isNotification);
    }

    TEST_F(ActionHelpersTest, RequireParamsValid) {
        const nlohmann::json params = {{"key", "value"}};
        auto command = API::InternalApi::Command{
            params,
            API::ApiId(API::getNextApiId()),
            API::InternalApi::Method(API::InternalApi::MethodTypes::SET),
            API::InternalApi::Target(API::InternalApi::TargetTypes::CORE)
        };

        ActionHelpers::CommandMetadata cmdMeta{
            command,
            {},
            API::getNextApiId()
        };

        const auto result = ActionHelpers::requireParams(cmdMeta);
        ASSERT_TRUE(result.has_value());
        ASSERT_NE(result.value(), nullptr);
        EXPECT_EQ(*result.value(), params);
    }

    TEST_F(ActionHelpersTest, RequireParamsInvalidOrMissing) {
        const auto command = API::InternalApi::Command{
            "invalid_params",
            API::ApiId(API::getNextApiId()),
            API::InternalApi::Method(API::InternalApi::MethodTypes::SET),
            API::InternalApi::Target(API::InternalApi::TargetTypes::CORE)
        };

        ActionHelpers::CommandMetadata cmdMeta{
            command,
            {},
            API::getNextApiId()
        };
        expectError(ActionHelpers::requireParams(cmdMeta), "params", "invalid prarms");

        cmdMeta.command.params.reset();
        ASSERT_EQ(cmdMeta.command.params, std::nullopt);
        expectError(ActionHelpers::requireParams(cmdMeta), "params", "missing params");
    }

    TEST_F(ActionHelpersTest, RequireTypeValid) {
        const nlohmann::json params = {{"type", "valid_type"}};

        const auto result = ActionHelpers::requireType(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["type"].get<std::string>());
    }

    TEST_F(ActionHelpersTest, RequireTypeInvalidOrMissing) {
        nlohmann::json params = {{"type", 1}};
        expectError(ActionHelpers::requireType(params), "type", "invalid type");

        params.erase("type");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requireType(params), "type", "missing type");
    }

    TEST_F(ActionHelpersTest, RequireModuleIdValid) {
        const nlohmann::json params = {{"module_id", 1}};

        const auto result = ActionHelpers::requireModuleId(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["module_id"].get<uint>());
    }

    TEST_F(ActionHelpersTest, RequireModuleIdInvalidOrMissing) {
        nlohmann::json params = {{"module_id", "invalid_id"}};
        expectError(ActionHelpers::requireModuleId(params), "module_id", "invalid module_id");

        params.erase("module_id");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requireModuleId(params), "module_id", "missing module_id");
    }

    TEST_F(ActionHelpersTest, RequireModuleIdDefaultsToZeroOnNegativeNumbers) {
        const nlohmann::json params = {{"module_id", -1}};

        const auto result = ActionHelpers::requireModuleId(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, 0);
    }

    TEST_F(ActionHelpersTest, RequireLimitArgValid) {
        const nlohmann::json params = {{"args", nlohmann::json::array({1, 2})}};

        const auto result = ActionHelpers::requireLimitArg(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["args"].back().get<uint>());
    }

    TEST_F(ActionHelpersTest, RequireLimitArgInvalidOrMissing) {
        nlohmann::json params = {{"args", 1}};
        expectError(ActionHelpers::requireLimitArg(params), "args", "args not an array");

        params["args"] = nlohmann::json::array({"invalid_limit_arg"});
        expectError(ActionHelpers::requireLimitArg(params), "args", "invalid limit arg");

        params["args"].clear();
        ASSERT_THAT(params["args"], ::testing::IsEmpty());
        expectError(ActionHelpers::requireLimitArg(params), "args", "empty args array");

        params.erase("args");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requireLimitArg(params), "args", "missing args");
    }

    TEST_F(ActionHelpersTest, RequireLimitArgDefaultsToZeroOnNegativeNumbers) {
        const nlohmann::json params = {{"args", nlohmann::json::array({1, -1})}};

        const auto result = ActionHelpers::requireLimitArg(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, 0);
    }

    TEST_F(ActionHelpersTest, RequireDeviceLogicIdArgValid) {
        const nlohmann::json params = {{"args", nlohmann::json::array({1, 2})}};

        const auto result = ActionHelpers::requireDeviceLogicIdArg(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["args"].front().get<uint>());
    }

    TEST_F(ActionHelpersTest, RequireDeviceLogicIdArgInvalidOrMissing) {
        nlohmann::json params = {{"args", 1}};
        expectError(ActionHelpers::requireDeviceLogicIdArg(params), "args", "args not an array");

        params["args"] = nlohmann::json::array({"invalid_device_id"});
        expectError(ActionHelpers::requireDeviceLogicIdArg(params), "args", "invalid device_id");

        params["args"].clear();
        ASSERT_THAT(params["args"], ::testing::IsEmpty());
        expectError(ActionHelpers::requireDeviceLogicIdArg(params), "args", "empty args array");

        params.erase("args");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requireDeviceLogicIdArg(params), "args", "missing args");
    }

    TEST_F(ActionHelpersTest, RequireDeviceLogicIdArgDefaultsToZeroOnNegativeNumbers) {
        const nlohmann::json params = {{"args", nlohmann::json::array({-1, 2})}};

        const auto result = ActionHelpers::requireDeviceLogicIdArg(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, 0);
    }

    TEST_F(ActionHelpersTest, ResolveDeviceIdValid) {
        nlohmann::json params = {{"args", {1, 2}}};

        auto result = ActionHelpers::resolveDeviceId(params, mConfigCache);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["args"].front().get<uint>());

        mConfigCache.setModule(CachedModule{1, 11, "name", {}});
        mConfigCache.setDevice(CachedDevice{2, 22, 1, "name", "sensor", {}});
        params = {{"args", {22, 1}}, {"module_id", 1}};

        result = ActionHelpers::resolveDeviceId(params, mConfigCache);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, 2);

        params = {{"args", {1, 2}}, {"module_id", "invalid_id"}}; // Ignoring invalid ID is valid behaviour

        result = ActionHelpers::resolveDeviceId(params, mConfigCache);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, 1);
    }

    TEST_F(ActionHelpersTest, ResolveDeviceIdInvalidOrMissing) {
        nlohmann::json params = {{"args", 1}};
        expectError(ActionHelpers::resolveDeviceId(params, mConfigCache), "args", "args not an array");

        params["args"] = nlohmann::json::array({"invalid_device_id"});
        expectError(ActionHelpers::resolveDeviceId(params, mConfigCache), "args", "invalid device_id");

        params["args"].clear();
        ASSERT_THAT(params["args"], ::testing::IsEmpty());
        expectError(ActionHelpers::resolveDeviceId(params, mConfigCache), "args", "empty args array");

        params.erase("args");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::resolveDeviceId(params, mConfigCache), "args", "missing args");

        params = {{"args", {1, 2}}, {"module_id", 2}};
        expectError(ActionHelpers::resolveDeviceId(params, mConfigCache), "not found", "device_id not found in cache",
                    API::ErrorCodes::NOT_FOUND);

        params = {{"args", {1, 2}}, {"module_id", -1}};
        expectError(ActionHelpers::resolveDeviceId(params, mConfigCache), "module [0]",
                    "negative module_id defaulted to 0",
                    API::ErrorCodes::NOT_FOUND);
    }

    TEST_F(ActionHelpersTest, RequireModeValid) {
        nlohmann::json params = {{"mode", "overwrite"}};
        auto result = ActionHelpers::requireMode(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["mode"].get<std::string>());

        params["mode"] = "append";
        result = ActionHelpers::requireMode(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["mode"].get<std::string>());
    }

    TEST_F(ActionHelpersTest, RequireModeInvalidOrMissing) {
        nlohmann::json params = {{"mode", 1}};
        expectError(ActionHelpers::requireMode(params), "mode", "invalid mode");

        params["mode"] = "invalid_mode";
        expectError(ActionHelpers::requireMode(params), "constants.h", "invalid mode value");

        params.erase("mode");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requireMode(params), "mode", "missing mode");
    }

    TEST_F(ActionHelpersTest, RequirePathValid) {
        nlohmann::json params = {{"path", "config.schedule.enabled"}};
        auto result = ActionHelpers::requirePath(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["path"].get<std::string>());

        params = {{"path", "modules"}};
        result = ActionHelpers::requirePath(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["path"].get<std::string>());
    }

    TEST_F(ActionHelpersTest, RequirePathInvalidOrMissing) {
        nlohmann::json params = {{"path", 1}};
        expectError(ActionHelpers::requirePath(params), "path", "invalid path");

        params.erase("path");
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requirePath(params), "path", "missing path");

        params["path"] = "config/schedule";
        expectError(ActionHelpers::requirePath(params), "First key", "invalid separator");

        params["path"] = "config.invalid.path";
        expectError(ActionHelpers::requirePath(params), "Nested keys", "invalid path value");
    }

    TEST_F(ActionHelpersTest, RequireValueValid) {
        nlohmann::json params = {{"value", "valid"}};
        auto result = ActionHelpers::requireValue(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["value"]);

        params["value"] = 1;
        result = ActionHelpers::requireValue(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["value"]);

        params["value"] = {{"key", "value"}};
        result = ActionHelpers::requireValue(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["value"]);

        params["value"] = {1, 2, 3};
        result = ActionHelpers::requireValue(params);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result, params["value"]);
    }

    TEST_F(ActionHelpersTest, RequireValueMissing) {
        const nlohmann::json params = nlohmann::json::object();
        ASSERT_THAT(params, ::testing::IsEmpty());
        expectError(ActionHelpers::requireValue(params), "value", "missing value");
    }
}
