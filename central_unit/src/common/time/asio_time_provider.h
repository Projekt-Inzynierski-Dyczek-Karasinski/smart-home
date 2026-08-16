#pragma once
#include "common/time/time_provider.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <utility>

namespace SmartHome::Time {
    /**
     * @brief ITimeProvider implementation backed by real clocks and Boost.Asio timers.
     */
    class AsioTimeProvider final : public ITimeProvider {
    public:
        /**
         * @brief Construct provider bound to an \c any_io_executor used by all created timers.
         *
         * @note The \c any_io_executor must outlive the provider and every timer it creates.
         */
        explicit AsioTimeProvider(boost::asio::any_io_executor executor)
            : mExecutor(std::move(executor)) {
        }

        /**
         * @brief Current wall-clock time.
         *
         * @return Current time according to the system clock.
         */
        [[nodiscard]] std::chrono::system_clock::time_point now() const override {
            return std::chrono::system_clock::now();
        }

        /**
         * @brief Current monotonic time (for elapsed-time measurements).
         *
         * @return Current time according to the steady clock.
         */
        [[nodiscard]] std::chrono::steady_clock::time_point steadyNow() const override {
            return std::chrono::steady_clock::now();
        }

        /**
         * @brief Create an absolute (wall-clock) timer.
         *
         * @return A new absolute timer instance bound to the provider's \c any_io_executor.
         */
        [[nodiscard]] std::unique_ptr<ITimer> createTimer() override {
            return std::make_unique<AsioTimer>(mExecutor);
        }

        /**
         * @brief Create a relative (monotonic) timer.
         *
         * @return A new relative timer instance bound to the provider's \c any_io_executor.
         */
        [[nodiscard]] std::unique_ptr<ISteadyTimer> createSteadyTimer() override {
            return std::make_unique<AsioSteadyTimer>(mExecutor);
        }

    private:
        /**
         * @brief Thin adapter over \c ba::system_timer.
         */
        class AsioTimer final : public ITimer {
        public:
            /**
             * @brief Construct a new \c AsioTimer object.
             *
             * @param executor The \c any_io_executor to which the timer is bound.
             */
            explicit AsioTimer(const boost::asio::any_io_executor &executor)
                : mTimer(executor) {
            }

            /**
             * @brief Set expiry time.
             *
             * @param timePoint The time point at which the timer should expire.
             *
             * @note Re-arming a timer with a pending wait cancels it: the pending handler
             *       is invoked with \c std::errc::operation_canceled (matching Asio semantics).
             */
            void expiresAt(const std::chrono::system_clock::time_point timePoint) override {
                mTimer.expires_at(timePoint);
            }

            /**
             * @brief Register a one-shot handler invoked on expiry or cancellation.
             *
             * @param handler The handler to invoke when the timer expires.
             */
            void asyncWait(TimerHandler handler) override {
                mTimer.async_wait([handler = std::move(handler)](const ::boost::system::error_code &ec) {
                    handler(ec);
                });
            }

            /**
             * @brief Cancel pending wait, its handler is invoked with \c std::errc::operation_canceled.
             */
            void cancel() override {
                mTimer.cancel();
            }

        private:
            boost::asio::system_timer mTimer;
        };

        /**
         * @brief Thin adapter over \c ba::steady_timer.
         */
        class AsioSteadyTimer final : public ISteadyTimer {
        public:
            /**
             * @brief Construct a new \c AsioSteadyTimer object.
             *
             * @param executor The \c any_io_executor to which the timer is bound.
             */
            explicit AsioSteadyTimer(const boost::asio::any_io_executor &executor)
                : mTimer(executor) {
            }

            /**
             * @brief Set expiry relative to now.
             *
             * @param delta The duration after which the timer should expire.
             *
             * @note Re-arming a timer with a pending wait cancels it: the pending handler
             *       is invoked with \c std::errc::operation_canceled (matching Asio semantics).
             */
            void expiresAfter(const std::chrono::steady_clock::duration delta) override {
                mTimer.expires_after(delta);
            }

            /**
             * @brief Register a one-shot handler invoked on expiry or cancellation.
             *
             * @param handler The handler to invoke when the timer expires.
             */
            void asyncWait(TimerHandler handler) override {
                mTimer.async_wait([handler = std::move(handler)](const ::boost::system::error_code &ec) {
                    handler(ec);
                });
            }

            /**
             * @brief Cancel pending wait, its handler is invoked with \c std::errc::operation_canceled.
             */
            void cancel() override {
                mTimer.cancel();
            }

        private:
            boost::asio::steady_timer mTimer;
        };

        boost::asio::any_io_executor mExecutor;
    };
}
