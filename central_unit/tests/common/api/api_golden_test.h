#pragma once
#include "api.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    class ApiGoldenTest : public ::testing::Test {
    protected:
        static nlohmann::json roundTrip(const API::ApiRequest &request);

        static nlohmann::json roundTrip(const API::ApiResponse &response);
    };
}
