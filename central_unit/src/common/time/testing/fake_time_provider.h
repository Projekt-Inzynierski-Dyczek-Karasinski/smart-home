#pragma once
#include "common/time/time_provider.h"

#include <memory>
#include <utility>
#include <vector>

namespace SmartHome::Time::Testing {
    /**
     * @brief ITimeProvider implementation for tests: manually advanced clocks
     *        driving both \c now() readings and timer expiry.
     *
     * @note Handlers never run on their own: they are queued and invoked inside
     *       \c advance...() / \c poll(), as Asio runs them only inside \c io_context::run()/poll().
     *       A handler queued by \c cancel() or re-arm fires on the next call, or is dropped if none follows.
     *
     * @note The provider must outlive every timer it creates (as \c io_context does in Asio).
     *
     * @note Not thread-safe.
     */
    class FakeTimeProvider final : public ITimeProvider {
    public:
        explicit FakeTimeProvider(const std::chrono::system_clock::time_point start)
            : mNow(start) {
        }

        [[nodiscard]] std::chrono::system_clock::time_point now() const override {
            return mNow;
        }

        [[nodiscard]] std::chrono::steady_clock::time_point steadyNow() const override {
            return mSteadyNow;
        }

        [[nodiscard]] std::unique_ptr<ITimer> createTimer() override {
            auto state = std::make_shared<State<std::chrono::system_clock> >();
            state->pReady = mpReady;
            mTimers.push_back(state);
            return std::make_unique<FakeTimer>(std::move(state));
        }

        [[nodiscard]] std::unique_ptr<ISteadyTimer> createSteadyTimer() override {
            auto state = std::make_shared<State<std::chrono::steady_clock> >();
            state->pReady = mpReady;
            mSteadyTimers.push_back(state);
            return std::make_unique<FakeSteadyTimer>(*this, std::move(state));
        }

        /// Advance both clocks by a delta and fire all due timers.
        void advanceBy(const std::chrono::nanoseconds delta) {
            mNow += delta;
            mSteadyNow += delta;
            fireDue();
        }

        /// Advance both clocks so that now() == timePoint, then fire all due timers.
        void advanceTo(const std::chrono::system_clock::time_point timePoint) {
            advanceBy(timePoint - mNow);
        }

        /// Move only the wall clock (simulates an NTP step / manual clock change).
        void advanceSystemOnly(const std::chrono::nanoseconds delta) {
            mNow += delta;
            fireDue();
        }

        /**
         * @brief Fire timers already due without moving the clocks.
         *
         * @details Fake counterpart of \c io_context::poll(). Needed when a timer was armed
         *          with an expiry already in the past: like Asio, the fake never invokes a
         *          handler from inside \c asyncWait().
         */
        void poll() {
            fireDue();
        }

    private:
        using PendingHandler = std::pair<TimerHandler, std::error_code>;
        using ReadyQueuePtr = std::shared_ptr<std::vector<PendingHandler> >;

        /**
         * @brief Shared timer state, outliving the timer object so a snapshot stays valid.
         */
        template<class Clock>
        struct State {
            Clock::time_point expiry{};
            TimerHandler handler;
            ReadyQueuePtr pReady;
            bool alive = true; ///< false once the owning timer has been destroyed

            void arm(TimerHandler newHandler) {
                handler = std::move(newHandler);
            }

            /// Queue pending handler with operation_canceled (cancel / re-arm / destroy path).
            void abort() {
                if (auto pending = std::exchange(handler, nullptr))
                    pReady->emplace_back(std::move(pending), std::make_error_code(std::errc::operation_canceled));
            }

            void fireIfDue(const Clock::time_point now) {
                if (handler && expiry <= now)
                    if (auto pending = std::exchange(handler, nullptr))
                        pReady->emplace_back(std::move(pending), std::error_code{});
            }
        };

        class FakeTimer final : public ITimer {
        public:
            explicit FakeTimer(std::shared_ptr<State<std::chrono::system_clock> > state)
                : mState(std::move(state)) {
            }

            /// Deregisters from the provider, pending handler is queued for the next fireDue().
            ~FakeTimer() override {
                mState->alive = false;
            }

            void expiresAt(const std::chrono::system_clock::time_point timePoint) override {
                mState->abort(); // Re-arm cancels a pending wait, as in Asio
                mState->expiry = timePoint;
            }

            void asyncWait(TimerHandler handler) override {
                mState->arm(std::move(handler));
            }

            void cancel() override {
                mState->abort();
            }

        private:
            std::shared_ptr<State<std::chrono::system_clock> > mState;
        };

        class FakeSteadyTimer final : public ISteadyTimer {
        public:
            FakeSteadyTimer(FakeTimeProvider &parent,
                            std::shared_ptr<State<std::chrono::steady_clock> > state)
                : mParent(parent), mState(std::move(state)) {
            }

            /// Deregisters from the provider; pending handler is queued for the next fireDue().
            ~FakeSteadyTimer() override {
                mState->alive = false;
            }

            void expiresAfter(const std::chrono::steady_clock::duration delta) override {
                mState->abort(); // Re-arm cancels a pending wait, as in Asio
                mState->expiry = mParent.mSteadyNow + delta;
            }

            void asyncWait(TimerHandler handler) override {
                mState->arm(std::move(handler));
            }

            void cancel() override {
                mState->abort();
            }

        private:
            FakeTimeProvider &mParent;
            std::shared_ptr<State<std::chrono::steady_clock> > mState;
        };

        /**
         * @brief Queue every timer whose expiry has passed then run handlers.
         *
         * @details Handlers are collected first and invoked only afterwards, so none runs while
         *          the caller of \c cancel() / \c expiresAt() may still hold a lock.
         */
        void fireDue() {
            const auto timers = mTimers;
            const auto steadyTimers = mSteadyTimers;

            for (const auto &state: timers) {
                if (state->alive) {
                    state->fireIfDue(mNow);
                } else {
                    state->abort();
                }
            }
            for (const auto &state: steadyTimers) {
                if (state->alive) {
                    state->fireIfDue(mSteadyNow);
                } else {
                    state->abort();
                }
            }

            std::erase_if(mTimers, [](const auto &state) { return !state->alive; });
            std::erase_if(mSteadyTimers, [](const auto &state) { return !state->alive; });

            drainReady();
        }

        /// Invoke queued handlers until none remain, re-entrant calls find the queue empty.
        void drainReady() {
            while (!mpReady->empty()) {
                std::vector<PendingHandler> batch;
                batch.swap(*mpReady);
                for (auto &[handler, ec]: batch) handler(ec);
            }
        }


        std::chrono::system_clock::time_point mNow;
        std::chrono::steady_clock::time_point mSteadyNow{};

        std::vector<std::shared_ptr<State<std::chrono::system_clock> > > mTimers;
        std::vector<std::shared_ptr<State<std::chrono::steady_clock> > > mSteadyTimers;
        ReadyQueuePtr mpReady = std::make_shared<std::vector<PendingHandler> >();
    };
}
