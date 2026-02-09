#pragma once
#include "cache.h"
#include "async_logger.h"

#include <queue>

#include <boost/asio.hpp>
#include <ical.h>


namespace SmartHome {
    /**
     * @brief Scheduled task execution engine using iCalendar RRULE recurrence.
     *
     * @details Maintains a priority queue of scheduled tasks parsed from sensor configurations.
     *          A single system_timer fires for the nearest task,
     *          dispatches the action through the existing Actions command pipeline,
     *          advances the RRULE iterator, and re-enqueues the task.
     *
     * @note All public methods are thread-safe.
     */
    class Scheduler {
    public:
        /**
         * @brief Construct scheduler bound to an io_context and config cache.
         *
         * @param ioContext Boost.Asio context for timer operations.
         * @param configCache Configuration cache for sensor schedule lookup.
         * @param logger Logger instance.
         */
        Scheduler(ba::io_context &ioContext,
                  const ConfigCache &configCache,
                  const std::shared_ptr<Utils::AsyncLogger> &logger);

        ~Scheduler();

        Scheduler(const Scheduler &) = delete;

        Scheduler &operator=(const Scheduler &) = delete;

        /**
         * @brief Load schedule entries from all sensors in config cache.
         *
         * @details Clears existing tasks and rebuilds the queue from current cache state.
         *
         * @pre Cache must be populated with sensor configurations containing "schedule" entries.
         */
        void loadFromCache();

        // TODO Unused for now, use after db trigger rework (adding payload with changed ids)
        /**
         * @brief Reload schedule entries for a specific sensor.
         *
         * @details Removes existing tasks for the sensor and reparses its schedule config.
         *
         * @note Used after LISTEN/NOTIFY config changes.
         *
         * @param sensorId Sensor identifier to reload.
         */
        void reloadSensor(uint sensorId);

        /**
         * @brief Remove all scheduled tasks for a sensor.
         *
         * @param sensorId Sensor identifier.
         */
        void removeSensor(uint sensorId);

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
         * @brief Number of tasks currently in the queue.
         */
        [[nodiscard]] size_t taskCount() const;

    private:
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
            uint sensorId; ///< Owning sensor
            nlohmann::json action; ///< Action definition from config
            std::chrono::system_clock::time_point nextRun; ///< Next scheduled execution time
            IcalIterPtr rruleIterator; ///< libical recurrence iterator
            bool removed = false; ///< delete flag for preventing dispatch of removed tasks

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
         * @brief Parse RRULE string and create iterator starting from now.
         *
         * @param rruleStr RRULE string (e.g. "FREQ=MINUTELY;INTERVAL=5").
         * @param dtstart Output parameter receiving the start time used.
         *
         * @return Iterator pointer or nullptr on parse failure.
         */
        static IcalIterPtr createRRuleIterator(const std::string &rruleStr,
                                               const icaltimetype &dtstart);

        /**
         * @brief Convert icaltimetype to system_clock time_point.
         */
        static std::chrono::system_clock::time_point icalToTimePoint(const icaltimetype &time);

        /**
         * @brief Parse sensor config \b schedule array and enqueue tasks.
         *
         * @param sensorId Sensor identifier.
         * @param config Sensor configuration JSON containing \b schedule key.
         */
        void parseSensorSchedule(uint sensorId, const nlohmann::json &config);

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
        void onTimerExpired(const boost::system::error_code &ec);

        /**
         * @brief Dispatch a scheduled action through \c Actions pipeline.
         *
         * @param task Task whose action to execute.
         */
        void dispatchAction(const TaskPtr &task) const;

        ba::io_context &mIoContext;
        ba::system_timer mTimer;
        const ConfigCache &mConfigCache;
        std::shared_ptr<Utils::AsyncLogger> mpLogger;

        TaskQueue mTaskQueue;
        mutable std::mutex mMutex;
        std::atomic_bool mIsRunning{false};
        std::atomic_bool mIsStoping{false};
    };
};
