#pragma once

#include "logger.h"

#include <boost/asio.hpp>

namespace ba = boost::asio;

namespace SmartHome::Utils {
    /**
     * @brief Thread-safe asynchronous logger for SmartHome system.
     *
     * @details Extends Logger with thread-safe asynchronous logging using Boost.Asio strand.
     *          All log operations are serialized through io_context::strand ensuring thread safety.
     *
     */
    class AsyncLogger final : public Logger {
    public:
        /**
         * @brief Construct AsyncLogger from existing Logger configuration.
         *
         * @details Copies logger configuration and applies it.
         *          If source logger has file logging enabled, continues logging to the same file.
         *
         * @param logger Shared pointer instance reference of logger to copy configuration from.
         * @param ioContext Boost.Asio io_context for async operations.
         *
         */
        AsyncLogger(const std::shared_ptr<Logger> &logger, ba::io_context &ioContext);

        /**
         * @brief Construct AsyncLogger with default configuration.
         *
         * @param ioContext Boost.Asio io_context for async operations.
         *
         */
        explicit AsyncLogger(ba::io_context &ioContext);

        /**
         * @brief Destructor ensures proper cleanup
         *
         * @note Pending async operations should be completed before destruction (io_context should be stopped).
         */
        ~AsyncLogger() override;

        /**
         * @brief Asynchronously log message with specified level.
         *
         * @param level Verbosity level of the message.
         * @param message Message to log.
         *
         * @note Overrides Logger::log to provide async behavior.
         *       Message is posted to strand for thread-safe processing.
         */
        void log(LogLevels::Level level, const std::string &message) override;

        template<typename... Args>
        void logf(LogLevels::Level level, const char *format, Args... args);

        template<typename... Args>
        void criticalf(const char *format, Args... args);

        template<typename... Args>
        void errorf(const char *format, Args... args);

        template<typename... Args>
        void warningf(const char *format, Args... args);

        template<typename... Args>
        void infof(const char *format, Args... args);

        template<typename... Args>
        void debugf(const char *format, Args... args);

    private:
        ba::io_context::strand mIoStrand;
    };
}

#include "async_logger.tpp"
