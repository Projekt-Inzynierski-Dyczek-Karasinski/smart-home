#pragma once
#include "actions/action_helpers.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace SmartHome::Tests {
    class ActionHelpersTest : public ::testing::Test {
    protected:
        template<typename T>
        void expectError(const ValidationResult<T> &result,
                         const std::string &substring,
                         const std::string &caseLabel,
                         API::ErrorCodes errorCode = API::ErrorCodes::INVALID_PARAMS) {
            SCOPED_TRACE(caseLabel);
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code, errorCode);
            EXPECT_THAT(result.error().data, ::testing::HasSubstr(substring));
        }

        ConfigCache mConfigCache;
    };
}
