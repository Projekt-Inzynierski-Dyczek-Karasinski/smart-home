#pragma once
#include <exception>
#include <string>

namespace SmartHome::Exceptions {
    /**
     * @brief Enum with custom SmartHome exception types.
     */
    enum class ExceptionCodes: int {
        INSTANCE_ALREADY_EXISTS,
        FILE_LOCK_FAILED,
        FILE_LOCK_REMOVE_FAILED,
        FILE_LOCK_OPEN_FAILED,
        FILE_LOCK_WRITE_FAILED,
    };

    /**
     * @brief Custom SmartHome exception class.
     */
    class Exception final : public std::exception {
        ExceptionCodes mCode;
        std::string mMessage;

    public:
        Exception(const ExceptionCodes code, const std::string_view message)
            : mCode(code), mMessage(message) {
        }

        /**
         * @brief Exception code getter.
         *
         * @return Exception code.
         */
        [[nodiscard]] ExceptionCodes getCode() const {
            return mCode;
        }

        /**
         * @brief Exception message getter.
         *
         * @return Exception message.
         */
        [[nodiscard]] const char *what() const noexcept override {
            return mMessage.c_str();
        }
    };
}
