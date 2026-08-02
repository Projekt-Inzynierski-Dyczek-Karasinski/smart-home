#pragma once
#include "cache.h"
#include "async_logger.h"
#include "actions/action_helpers.h"
#include "common/time/time_provider.h"

#include <queue>
#include <unordered_map>
#include <shared_mutex>
#include <memory>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <ical.h>

namespace SmartHome {
    namespace ba = boost::asio;

    /**
     * @brief Scheduled task execution engine using iCalendar RRULE recurrence.
     *
     * @details Maintains a priority queue of tasks parsed from device configurations, plus a
     *          per-device index used for removal. A single ITimer fires for the nearest task,
     *          dispatches the action through the existing Actions command pipeline,
     *          advances the RRULE iterator, and re-enqueues the task.
     *
     * @note All public methods are thread-safe.
     */
    class Scheduler : public std::enable_shared_from_this<Scheduler> {
    public:
        /**
         * @brief Creates an new instance of \c Scheduler.
         *
         * @param ioContext Boost.Asio context for timer operations.
         * @param configCache Configuration cache for device schedule lookup.
         * @param logger Logger instance.
         * @param timeProvider Time source abstraction used for scheduling calculations (injectable for testing).
         * @param dispatchAction Callable used to dispatch device actions (injectable for testing).
         *
         * @return A new instance of \c Scheduler. Ownership is transferred to the caller.
         */
        [[nodiscard]] static std::shared_ptr<Scheduler> create(
            ba::io_context &ioContext,
            const ConfigCache &configCache,
            const std::shared_ptr<Utils::AsyncLogger> &logger,
            Time::ITimeProvider &timeProvider,
            ActionDispatcher dispatchAction = &ActionHelpers::dispatchAutomatedDeviceAction
        );

        /**
         * @brief Destructor. Calls \c stop().
         */
        ~Scheduler();

        Scheduler(const Scheduler &) = delete;

        Scheduler &operator=(const Scheduler &) = delete;

        /**
         * @brief Load schedule entries from all devices in config cache.
         *
         * @details Clears existing tasks and rebuilds the queue from current cache state.
         *
         * @pre Cache must be populated with device configurations containing "schedule" entries.
         */
        void loadFromCache();

        /**
         * @brief Marks tasks as removed in \c mTaskQueue for lazy deletion and deletes them from \c mTasksByDevice.
         *
         * @param deviceId Device identifier.
         */
        void removeDevice(uint deviceId);

        /**
         * @brief Start the scheduling timer chain.
         */
        void start();

        /**
         * @brief Stop scheduling and cancel pending timer.
         */
        void stop();

        /**
         * @brief Check if scheduler is currently running.
         */
        [[nodiscard]] bool isRunning() const;

        /**
         * @brief Number of live tasks (excludes removed and exhausted ones).
         */
        [[nodiscard]] size_t taskCount() const;

        /**
         * @brief Get the next scheduled run time for any task belonging to a module.
         *
         * @return Time point of the next scheduled run for the module, or std::nullopt if no tasks are scheduled for it.
         */
        std::optional<std::chrono::system_clock::time_point> getNextRunForModule(uint moduleId) const;

    private:
        /**
         * @brief Construct scheduler bound to an io_context and config cache.
         *
         * @note This constructor is private and only used internally.
         *       Use the static create() method to instantiate a scheduler.
         *
         * @param ioContext Boost.Asio context for timer operations.
         * @param configCache Configuration cache for device schedule lookup.
         * @param logger Logger instance.
         * @param timeProvider Time source abstraction used for scheduling calculations (injectable for testing).
         * @param dispatchAction Callable used to dispatch device actions (injectable for testing).
         */
        Scheduler(ba::io_context &ioContext,
                  const ConfigCache &configCache,
                  const std::shared_ptr<Utils::AsyncLogger> &logger,
                  Time::ITimeProvider &timeProvider,
                  ActionDispatcher dispatchAction);

        /**
         * @brief RAII wrapper for icalrecur_iterator lifecycle.
         */
        struct IcalIterDeleter {
            void operator()(icalrecur_iterator *iter) const {
                if (iter) icalrecur_iterator_free(iter);
            }
        };

        using IcalIterPtr = std::unique_ptr<icalrecur_iterator, IcalIterDeleter>;

        /**
         * @brief Single scheduled task with RRULE iterator state.
         */
        struct ScheduledTask {
            uint deviceId; ///< Owning device
            nlohmann::json action; ///< Action definition from config
            std::chrono::system_clock::time_point nextRun; ///< Next scheduled execution time
            IcalIterPtr rruleIterator; ///< libical recurrence iterator
            bool removed = false; ///< Lazy-delete flag: skipped in the queue and suppresses dispatch

            /**
             * @brief Advance iterator to next occurrence.
             *
             * @return true if next occurrence exists, false if recurrence ended.
             */
            bool advanceToNext();
        };

        using TaskPtr = std::shared_ptr<ScheduledTask>;

        /**
         * @brief Comparator for min-heap ordering by nextRun (earliest first).
         */
        struct TaskComparator {
            bool operator()(const TaskPtr &a, const TaskPtr &b) const {
                return a->nextRun > b->nextRun;
            }
        };

        using TaskQueue = std::priority_queue<TaskPtr, std::vector<TaskPtr>, TaskComparator>;

        /**
         * @brief Parse RRULE string and create an iterator anchored at dtstart.
         *
         * @param rruleStr RRULE string (e.g. "FREQ=MINUTELY;INTERVAL=5").
         * @param dtstart Start time for the recurrence iterator.
         *
         * @return Iterator pointer or nullptr on parse failure.
         */
        static IcalIterPtr createRRuleIterator(const std::string &rruleStr,
                                               const icaltimetype &dtstart);

        /**
         * @brief Convert \c icaltimetype to \c system_clock::time_point.
         */
        static std::chrono::system_clock::time_point icalToTimePoint(const icaltimetype &time);

        /**
         * @brief Convert \c system_clock::time_point to \c icaltimetype.
         */
        static icaltimetype timePointToIcal(std::chrono::system_clock::time_point timePoint);

        /**
         * @brief Parse device config \b schedule array and enqueue tasks.
         *
         * @param deviceId Device identifier.
         * @param config Device configuration JSON containing \b schedule key.
         */
        void parseDeviceSchedule(uint deviceId, const nlohmann::json &config);

        /**
         * @brief Set timer to fire at the earliest task's nextRun.
         *
         * @note Must be called with mMutex held or from timer handler.
         */
        void scheduleNextTimer();

        /**
         * @brief Timer expiration handler.
         *
         * @details Dispatch ready tasks and re-arm.
         *
         * @param ec Error code from async_wait.
         */
        void onTimerExpired(const std::error_code &ec);

        /**
         * @brief Dispatch a scheduled action through \c Actions pipeline.
         *
         * @param pTask Task whose action to execute.
         */
        void dispatchAction(const TaskPtr &pTask) const;

        /**
         * @brief Register a newly parsed task in both the queue and the device index.
         *
         * @param pTask Task to register.
         */
        void enqueueTask(const TaskPtr &pTask);

        /**
         * @brief Return an already-indexed task to the queue after advancing its iterator.
         *
         * @param pTask Task to requeue.
         *
         * @pre The task's iterator is advanced before requeue.
         */
        void requeueTask(const TaskPtr &pTask);

        /**
         * @brief Remove a task from the device index.
         *
         * @param pTask Task to remove.
         *
         * @note A dispatch already posted for this task still runs, \c removeDevice() can no longer suppress it,
         *       as the task is no longer reachable from the index.
         */
        void retireTask(const TaskPtr &pTask);

        mutable std::shared_mutex mMutex;

        ba::io_context &mIoContext;
        const ConfigCache &mConfigCache;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;
        Time::ITimeProvider &mTimeProvider;

        ActionDispatcher mDispatchAction; ///< Device action dispatcher (injectable for testing)

        std::unique_ptr<Time::ITimer> mpTimer;
        TaskQueue mTaskQueue; /// All tasks including retired ones pending removal.
        std::unordered_map<uint, std::vector<TaskPtr> > mTasksByDevice; /// Live tasks by device.
        std::atomic_bool mIsRunning{false};
    };
};
