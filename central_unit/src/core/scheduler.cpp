#include "scheduler.h"
#include "constants.h"

#include <ranges>

namespace SmartHome {
    namespace c = Constants;

    using namespace std::string_literals;

    std::shared_ptr<Scheduler> Scheduler::create(ba::io_context &ioContext,
                                                 const ConfigCache &configCache,
                                                 const std::shared_ptr<Utils::AsyncLogger> &logger,
                                                 Time::ITimeProvider &timeProvider,
                                                 ActionDispatcher dispatchAction) {
        return std::shared_ptr<Scheduler>(
            new Scheduler(ioContext, configCache, logger, timeProvider, std::move(dispatchAction)));
    }

    Scheduler::~Scheduler() {
        stop();
    }

    void Scheduler::loadFromCache() {
        std::unique_lock lock(mMutex);

        // Drop all tasks: queue by swap, index by clear
        TaskQueue empty;
        std::swap(mTaskQueue, empty);
        mTasksByDevice.clear();

        for (const auto &device: mConfigCache.getAllDevices()) {
            parseDeviceSchedule(device.id, device.config);
        }
        mpLogger->infof("[SCHEDULER] Loaded %zu tasks from cache", mTaskQueue.size());

        if (mIsRunning) scheduleNextTimer();
    }

    void Scheduler::removeDevice(const uint deviceId) {
        std::unique_lock lock(mMutex);

        if (const auto iter = mTasksByDevice.find(deviceId); iter != mTasksByDevice.end()) {
            for (const auto &pTask: iter->second)
                pTask->removed = true;
            mTasksByDevice.erase(iter);
        }

        if (mIsRunning) scheduleNextTimer();
    }

    void Scheduler::start() {
        if (mIsRunning.exchange(true, std::memory_order_acq_rel)) return;

        std::unique_lock lock(mMutex);
        mpLogger->info("[SCHEDULER] Starting scheduler");
        scheduleNextTimer();
    }

    void Scheduler::stop() {
        if (!mIsRunning.exchange(false, std::memory_order_acq_rel)) return;

        std::unique_lock lock(mMutex);
        mpLogger->info("[SCHEDULER] Stopping scheduler");
        mpTimer->cancel();
    }

    bool Scheduler::isRunning() const {
        return mIsRunning.load(std::memory_order::acquire);
    }

    size_t Scheduler::taskCount() const {
        std::shared_lock lock(mMutex);

        size_t count = 0;
        for (const auto &deviceTasks: mTasksByDevice | std::views::values) {
            count += deviceTasks.size();
        }

        return count;
    }

    std::optional<std::chrono::system_clock::time_point> Scheduler::getNextRunForModule(const uint moduleId) const {
        std::shared_lock lock(mMutex);

        auto queueCopy = mTaskQueue;
        std::optional<std::chrono::system_clock::time_point> earliest;

        while (!queueCopy.empty()) {
            if (const auto &pTask = queueCopy.top(); !pTask->removed) {
                if (const auto &device = mConfigCache.getDevice(pTask->deviceId);
                    device.has_value() && device->moduleId == moduleId) {
                    earliest = pTask->nextRun;
                    break;
                }
            }

            queueCopy.pop();
        }

        return earliest;
    }

    Scheduler::Scheduler(ba::io_context &ioContext,
                         const ConfigCache &configCache,
                         const std::shared_ptr<Utils::AsyncLogger> &logger,
                         Time::ITimeProvider &timeProvider,
                         ActionDispatcher dispatchAction)
        : mIoContext(ioContext),
          mConfigCache(configCache),
          mpLogger(logger),
          mTimeProvider(timeProvider),
          mDispatchAction(std::move(dispatchAction)) {
        mpTimer = mTimeProvider.createTimer();
    }

    bool Scheduler::ScheduledTask::advanceToNext() {
        if (!rruleIterator) return false;

        const icaltimetype next = icalrecur_iterator_next(rruleIterator.get());
        if (icaltime_is_null_time(next)) return false;

        nextRun = icalToTimePoint(next);
        return true;
    }

    Scheduler::IcalIterPtr Scheduler::createRRuleIterator(const std::string &rruleStr, const icaltimetype &dtstart) {
        const auto recurrence = icalrecurrencetype_from_string(rruleStr.c_str());
        if (recurrence.freq == ICAL_NO_RECURRENCE) {
            return nullptr;
        }

        const auto iter = icalrecur_iterator_new(recurrence, dtstart);
        if (!iter) return nullptr;

        return IcalIterPtr(iter);
    }

    std::chrono::system_clock::time_point Scheduler::icalToTimePoint(const icaltimetype &time) {
        const time_t timestamp = icaltime_as_timet_with_zone(time, icaltimezone_get_utc_timezone());
        return std::chrono::system_clock::from_time_t(timestamp);
    }

    icaltimetype Scheduler::timePointToIcal(const std::chrono::system_clock::time_point timePoint) {
        const auto seconds = std::chrono::floor<std::chrono::seconds>(timePoint);
        constexpr bool isDate = false;
        return icaltime_from_timet_with_zone(std::chrono::system_clock::to_time_t(seconds),
                                             isDate,
                                             icaltimezone_get_utc_timezone());
    }

    void Scheduler::parseDeviceSchedule(const uint deviceId, const nlohmann::json &config) {
        if (!config.contains(c::DeviceConfigKeys::SCHEDULE)) {
            return; // Skip devices without schedule field
        }
        if (!config[c::DeviceConfigKeys::SCHEDULE].is_array()) {
            mpLogger->errorf(
                ("[SCHEDULER] Skipped parsing scheduled events for device [%u]: "
                    "'%s' device config field must be an array"),
                deviceId,
                c::DeviceConfigKeys::SCHEDULE.data());
            return;
        }

        const std::string errorLogStr = "[SCHEDULER] Failed to parse scheduled event for device ["s +
                                        std::to_string(deviceId) + "]: ";

        const auto now = mTimeProvider.now();
        const auto icalNow = timePointToIcal(now);

        for (const auto &entry: config[c::DeviceConfigKeys::SCHEDULE]) {
            if (!entry.is_object()) {
                mpLogger->error(errorLogStr + "scheduled event entry must be an object");
                continue;
            }
            if (!entry.contains(c::DeviceConfigKeys::ENABLED) || !entry[c::DeviceConfigKeys::ENABLED].is_boolean()) {
                mpLogger->error(errorLogStr + "scheduled event entry must contain boolean '"
                                + c::DeviceConfigKeys::ENABLED.data() + "' field");
                continue;
            }
            if (!entry.contains(c::DeviceConfigKeys::RRULE) || !entry[c::DeviceConfigKeys::RRULE].is_string()) {
                mpLogger->error(errorLogStr + "scheduled event entry must contain string '"
                                + c::DeviceConfigKeys::RRULE.data() + "' field");
                continue;
            }
            if (!entry.contains(c::DeviceConfigKeys::ACTION) || !entry[c::DeviceConfigKeys::ACTION].is_object()) {
                mpLogger->error(errorLogStr + "scheduled event entry must contain an '"
                                + c::DeviceConfigKeys::ACTION.data() + "' object");
                continue;
            }

            if (!entry[c::DeviceConfigKeys::ENABLED].get<bool>()) continue; // skip disabled events

            icaltimetype dtstart = icaltime_null_time();
            if (entry.contains(c::DeviceConfigKeys::DTSTART)) {
                if (entry[c::DeviceConfigKeys::DTSTART].is_string()) {
                    dtstart = icaltime_from_string(entry[c::DeviceConfigKeys::DTSTART].get<std::string>().c_str());
                }
                if (icaltime_is_null_time(dtstart)) {
                    mpLogger->errorf("[SCHEDULER] Failed to parse '%s' (%s) for device [%u], "
                                     "defaulting to now and skipping first occurrence",
                                     c::DeviceConfigKeys::DTSTART.data(),
                                     entry[c::DeviceConfigKeys::DTSTART].dump().c_str(),
                                     deviceId);
                }
            } else {
                mpLogger->debugf(
                    "[SCHEDULER] No 'dtstart' for device [%u], defaulting to now and skipping first occurrence",
                    deviceId);
            }

            if (icaltime_is_null_time(dtstart)) dtstart = icalNow;

            const auto &rruleStr = entry[c::DeviceConfigKeys::RRULE].get<std::string>();
            auto rruleIter = createRRuleIterator(rruleStr, dtstart);
            if (!rruleIter) {
                mpLogger->errorf("[SCHEDULER] Failed to parse RRULE '%s' for device [%u]", rruleStr.c_str(), deviceId);
                continue;
            }

            auto pTask = std::make_shared<ScheduledTask>();
            pTask->deviceId = deviceId;
            pTask->action = entry[c::DeviceConfigKeys::ACTION];
            pTask->rruleIterator = std::move(rruleIter);

            // Fast forward if possible
            if (icaltime_compare(dtstart, icalNow) < 0) {
                if (!icalrecur_iterator_set_start(pTask->rruleIterator.get(), icalNow)) {
                    mpLogger->debugf("[SCHEDULER] Fast-forward unavailable for device [%u], RRULE: %s\n"
                                     "RRULE with COUNT is not supported by fast-forward\n "
                                     "Check RRULE if errors occur",
                                     deviceId, rruleStr.c_str());
                }
            }

            // Advance to first future occurrence
            if (pTask->advanceToNext()) {
                // If nextRun is in the past, advance until it's in the future or recurrence ends
                uint iterations = 0;
                while (pTask->nextRun <= now) {
                    if (!pTask->advanceToNext()) {
                        break; // No next recurrence
                    }
                    if (constexpr uint iterationLimit = 20'000; ++iterations > iterationLimit) {
                        mpLogger->errorf("[SCHEDULER] Failed to advance to next task for device [%u]: "
                                         "reached iteration limit, dtstart may be too far in the past",
                                         deviceId);
                        break; // Reached iteration limit
                    }
                }

                if (pTask->nextRun > now) {
                    mpLogger->debugf("[SCHEDULER] Enqueued task for device [%u], next run in %lld s",
                                     deviceId,
                                     std::chrono::duration_cast<std::chrono::seconds>(pTask->nextRun - now).count());
                    enqueueTask(pTask);
                }
            }
        }
    }

    void Scheduler::scheduleNextTimer() {
        // Skip removed tasks
        while (!mTaskQueue.empty() && mTaskQueue.top()->removed) {
            mTaskQueue.pop();
        }

        if (mTaskQueue.empty()) {
            mpLogger->debug("[SCHEDULER] No tasks in queue");
            return;
        }

        const auto &nextTask = mTaskQueue.top();
        mpTimer->expiresAt(nextTask->nextRun);
        mpTimer->asyncWait([self = weak_from_this()](const std::error_code &ec) {
            if (const auto p = self.lock()) p->onTimerExpired(ec);
        });
    }

    void Scheduler::onTimerExpired(const std::error_code &ec) {
        if (ec || !mIsRunning) return;

        std::unique_lock lock(mMutex);
        const auto now = mTimeProvider.now();

        while (!mTaskQueue.empty()) {
            // Skip and remove tasks that were flagged as removed
            if (mTaskQueue.top()->removed) {
                mTaskQueue.pop();
                continue;
            }

            // Break if the earliest task is not ready yet
            if (mTaskQueue.top()->nextRun > now) break;

            auto pTask = mTaskQueue.top();
            mTaskQueue.pop();

            // Advance to next occurrence before dispatching current one, retire task if it has no more occurrences
            if (pTask->advanceToNext()) {
                requeueTask(pTask);
            } else {
                retireTask(pTask);
            }

            dispatchAction(pTask);
        }

        // Set timer for next occurrence
        scheduleNextTimer();
    }

    void Scheduler::dispatchAction(const TaskPtr &pTask) const {
        if (!mIsRunning) return;
        mpLogger->debugf("[SCHEDULER] Dispatching %s for device [%u]",
                         c::AutomatedActionNames::SCHEDULED_ACTION.data(), pTask->deviceId);

        boost::asio::post(mIoContext, [self = shared_from_this(), pTask] {
            if (!self->mIsRunning || pTask->removed) return;
            self->mDispatchAction(c::AutomatedActionNames::SCHEDULED_ACTION, pTask->deviceId, pTask->action);
        });
    }

    void Scheduler::enqueueTask(const TaskPtr &pTask) {
        mTaskQueue.push(pTask);
        mTasksByDevice[pTask->deviceId].push_back(pTask);
    }

    void Scheduler::requeueTask(const TaskPtr &pTask) {
        mTaskQueue.push(pTask);
    }

    void Scheduler::retireTask(const TaskPtr &pTask) {
        const auto iter = mTasksByDevice.find(pTask->deviceId);
        if (iter == mTasksByDevice.end()) return;

        std::erase(iter->second, pTask);
        if (iter->second.empty()) mTasksByDevice.erase(iter);
    }
}
