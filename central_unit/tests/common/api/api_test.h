#pragma once
#include "api.h"

#include <gtest/gtest.h>

namespace SmartHome::Tests {
    class ApiTest : public ::testing::Test {
    protected:
        struct InvalidJsonCase {
            nlohmann::json json;
            std::string expectedMessage;
        };

        static std::vector<nlohmann::json> validErrorJsons();

        static std::vector<nlohmann::json> validRequestJsons();

        static std::vector<nlohmann::json> validResponseJsons();

        static constexpr apiId_t msTEST_ID = 3;
    };
}
