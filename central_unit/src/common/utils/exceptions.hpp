#pragma once
#include <exception>
#include <string>

namespace SmartHome::Exceptions {
    enum class ExceptionCodes: int {
        INSTANCE_ALREADY_EXISTS,
        FILE_LOCK_FAILED,
        FILE_LOCK_REMOVE_FAILED,
        FILE_LOCK_OPEN_FAILED,
        FILE_LOCK_WRITE_FAILED,
    };

    class Exception final : public std::exception {
        ExceptionCodes mCode;
        std::string mMessage;

    public:
        Exception(const ExceptionCodes code, const std::string_view message)
            : mCode(code), mMessage(message) {
        }


        ExceptionCodes getCode() const {
            return mCode;
        }

        const char *what() const noexcept override {
            return mMessage.c_str();
        }
    };
}
