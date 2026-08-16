#pragma once
#include <chrono>
#include <functional>
#include <memory>

#include <system_error>

// TODO add time zone change check and handling

namespace SmartHome::Time {
    /**
     * @brief Handler invoked on timer expiry or cancellation.
     *
     * @details Receives a default-constructed error code on normal expiry, or an error code
     *          comparing equal to \c std::errc::operation_canceled on cancellation or re-arm.
     */
    using TimerHandler = std::function<void(const std::error_code &)>;

    /**
     * @brief Absolute timer bound to \c std::chrono::system_clock.
     *
     * @details Intended for calendar-driven scheduling (RRULE, absolute deadlines).
     */
    struct ITimer {
        virtual ~ITimer() = default;

        /**
         * @brief Set expiry time.
         *
         * @note Re-arming a timer with a pending wait cancels it: the pending handler
         *       is invoked with \c std::errc::operation_canceled (matching Asio semantics).
         */
        virtual void expiresAt(std::chrono::system_clock::time_point timePoint) = 0;

        /**
         * @brief Register a one-shot handler invoked on expiry or cancellation.
         */
        virtual void asyncWait(TimerHandler handler) = 0;

        /**
         * @brief Cancel pending wait, its handler is invoked with \c std::errc::operation_canceled.
         */
        virtual void cancel() = 0;
    };

    /**
     * @brief Relative timer bound to \c std::chrono::steady_clock.
     *
     * @details Intended for timeouts and retries. Immune to wall-clock jumps.
     */
    struct ISteadyTimer {
        virtual ~ISteadyTimer() = default;

        /**
         * @brief Set expiry relative to now.
         *
         * @note Re-arming a timer with a pending wait cancels it: the pending handler
         *       is invoked with \c std::errc::operation_canceled (matching Asio semantics).
         */
        virtual void expiresAfter(std::chrono::steady_clock::duration delta) = 0;

        /**
         * @brief Register a one-shot handler invoked on expiry or cancellation.
         */
        virtual void asyncWait(TimerHandler handler) = 0;

        /**
         * @brief Cancel pending wait, its handler is invoked with \c std::errc::operation_canceled.
         */
        virtual void cancel() = 0;
    };

    /**
     * @brief Single source of truth for time: clock readings and timer factory.
     */
    struct ITimeProvider {
        virtual ~ITimeProvider() = default;

        /**
         * @brief Current wall-clock time.
         */
        [[nodiscard]] virtual std::chrono::system_clock::time_point now() const = 0;

        /**
         * @brief Current monotonic time (for elapsed-time measurements).
         */
        [[nodiscard]] virtual std::chrono::steady_clock::time_point steadyNow() const = 0;

        /**
         * @brief Create an absolute (wall-clock) timer.
         */
        [[nodiscard]] virtual std::unique_ptr<ITimer> createTimer() = 0;

        /**
         * @brief Create a relative (monotonic) timer.
         */
        [[nodiscard]] virtual std::unique_ptr<ISteadyTimer> createSteadyTimer() = 0;
    };
}
