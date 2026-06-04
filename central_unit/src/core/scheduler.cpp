#include "scheduler.h"
#include "constants.h"
#include "api/internal_api.h"
#include "actions/actions.h"
#include "actions/database_actions.h"

namespace SmartHome {
    namespace jrs = JsonRpcStrings;
    namespace c = Constants;

    Scheduler::Scheduler(ba::io_context &ioContext,
                         const ConfigCache &configCache,
                         const std::shared_ptr<Utils::AsyncLogger> &logger)
        : mIoContext(ioContext),
          mTimer(ioContext),
          mConfigCache(configCache),
          mpLogger(logger) {
    }

    Scheduler::~Scheduler() {
        if (mIsStoping.exchange(true, std::memory_order_acq_rel)) return;
        stop();
    }

    void Scheduler::loadFromCache() {
        std::scoped_lock lock(mMutex);

        // Clear existing tasks queue by swapping with an empty queue
        TaskQueue empty;
        std::swap(mTaskQueue, empty);

        for (const auto &device: mConfigCache.getAllDevices()) {
            parseDeviceSchedule(device.id, device.config);
        }
        mpLogger->infof("[SCHEDULER] Loaded %zu tasks from cache", mTaskQueue.size());

        if (mIsRunning) scheduleNextTimer();
    }

    void Scheduler::reloadDevice(const uint deviceId) {
        removeDevice(deviceId);

        std::scoped_lock lock(mMutex);

        // Reparse device schedule from current cache state
        const auto deviceOpt = mConfigCache.getDevice(deviceId);
        if (deviceOpt.has_value()) {
            parseDeviceSchedule(deviceId, deviceOpt->config);
        }

        mpLogger->debugf("[SCHEDULER] Reloaded schedule for device [%u], queue size: %zu",
                         deviceId, mTaskQueue.size());

        if (mIsRunning) scheduleNextTimer();
    }

    void Scheduler::removeDevice(const uint deviceId) {
        std::scoped_lock lock(mMutex);

        // Clear existing tasks queue by swapping with an empty queue
        TaskQueue tempQueue;
        std::swap(mTaskQueue, tempQueue);

        // Filter out tasks for the device and keep the rest, flag removed tasks to avoid dispatching them
        while (!tempQueue.empty()) {
            auto task = tempQueue.top();
            tempQueue.pop();
            if (task->deviceId == deviceId) {
                task->removed = true;
            } else {
                mTaskQueue.push(std::move(task));
            }
        }

        mpLogger->debugf("[SCHEDULER] Removed tasks for device [%u], queue size: %zu",
                         deviceId, mTaskQueue.size());

        if (mIsRunning) scheduleNextTimer();
    }

    void Scheduler::start() {
        if (mIsRunning.exchange(true, std::memory_order_acq_rel)) return;

        std::scoped_lock lock(mMutex);
        mpLogger->info("[SCHEDULER] Starting scheduler");
        scheduleNextTimer();
    }

    void Scheduler::stop() {
        if (!mIsRunning.exchange(false, std::memory_order_acq_rel)) return;

        std::scoped_lock lock(mMutex);
        mpLogger->info("[SCHEDULER] Stopping scheduler");
        mTimer.cancel();
    }

    bool Scheduler::isRunning() const {
        return mIsRunning.load(std::memory_order::acquire);
    }

    size_t Scheduler::taskCount() const {
        std::scoped_lock lock(mMutex);
        return mTaskQueue.size();
    }

    std::optional<std::chrono::system_clock::time_point> Scheduler::getNextRunForModule(const uint moduleId) const {
        std::scoped_lock lock(mMutex);

        auto queueCopy = mTaskQueue;
        std::optional<std::chrono::system_clock::time_point> earliest;

        while (!queueCopy.empty()) {
            const auto &task = queueCopy.top();

            if (!task->removed) {
                const auto device = mConfigCache.getDevice(task->deviceId);
                if (device.has_value() && device->moduleId == moduleId) {
                    earliest = task->nextRun;
                    break;
                }
            }

            queueCopy.pop();
        }

        return earliest;
    }

    bool Scheduler::ScheduledTask::advanceToNext() {
        if (!rruleIterator) return false;

        const icaltimetype next = icalrecur_iterator_next(rruleIterator.get());
        if (icaltime_is_null_time(next)) return false;

        nextRun = icalToTimePoint(next);
        return true;
    }

    Scheduler::IcalIterPtr Scheduler::createRRuleIterator(const std::string &rruleStr, const icaltimetype &dtstart) {
        // Parse RRULE string
        icalrecurrencetype recurrence;
        icalrecurrencetype_clear(&recurrence); // Zero-initialize
        recurrence = icalrecurrencetype_from_string(rruleStr.c_str());

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

    void Scheduler::parseDeviceSchedule(const uint deviceId, const nlohmann::json &config) {
        if (!config.contains(c::DeviceConfigKeys::SCHEDULE) || !config[c::DeviceConfigKeys::SCHEDULE].is_array()) {
            return;
        }

        for (const auto &entry: config[c::DeviceConfigKeys::SCHEDULE]) {
            if (!entry.is_object()) {
                continue;
            }
            if (entry.contains(c::DeviceConfigKeys::ENABLED) &&
                entry[c::DeviceConfigKeys::ENABLED].is_boolean() &&
                !entry[c::DeviceConfigKeys::ENABLED].get<bool>()) {
                continue;
            }
            if (!entry.contains(c::DeviceConfigKeys::RRULE) || !entry[c::DeviceConfigKeys::RRULE].is_string()) {
                continue;
            }
            if (!entry.contains(c::DeviceConfigKeys::ACTION) || !entry[c::DeviceConfigKeys::ACTION].is_object()) {
                continue;
            }

            icaltimetype dtstart;
            if (entry.contains(c::DeviceConfigKeys::DTSTART) && entry[c::DeviceConfigKeys::DTSTART].is_string()) {
                dtstart = icaltime_from_string(entry[c::DeviceConfigKeys::DTSTART].get<std::string>().c_str());
                if (icaltime_is_null_time(dtstart)) {
                    dtstart = icaltime_current_time_with_zone(icaltimezone_get_utc_timezone());
                }
            } else {
                dtstart = icaltime_current_time_with_zone(icaltimezone_get_utc_timezone());
            }

            const auto &rruleStr = entry[c::DeviceConfigKeys::RRULE].get<std::string>();
            auto rruleIter = createRRuleIterator(rruleStr, dtstart);
            if (!rruleIter) {
                mpLogger->errorf("[SCHEDULER] Failed to parse RRULE '%s' for device [%u]", rruleStr.c_str(), deviceId);
                continue;
            }

            auto task = std::make_shared<ScheduledTask>();
            task->deviceId = deviceId;
            task->action = entry[c::DeviceConfigKeys::ACTION];
            task->rruleIterator = std::move(rruleIter);

            // Advance to first future occurrence
            if (task->advanceToNext()) {
                const auto now = std::chrono::system_clock::now();
                // If nextRun is in the past, advance until it's in the future or recurrence ends
                while (task->nextRun <= now) {
                    if (!task->advanceToNext()) break;
                }

                if (task->nextRun > now) {
                    mpLogger->debugf("[SCHEDULER] Enqueued task for device [%u], next run in %lld s",
                                     deviceId,
                                     std::chrono::duration_cast<std::chrono::seconds>(task->nextRun - now).count());
                    mTaskQueue.push(std::move(task));
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
            mpLogger->debugf("[SCHEDULER] No task in queue");
            return;
        }

        const auto &nextTask = mTaskQueue.top();
        mTimer.expires_at(nextTask->nextRun);
        mTimer.async_wait([this](const boost::system::error_code &ec) {
            onTimerExpired(ec);
        });
    }

    void Scheduler::onTimerExpired(const boost::system::error_code &ec) {
        if (ec || !mIsRunning) return;

        std::scoped_lock lock(mMutex);
        const auto now = std::chrono::system_clock::now();

        while (!mTaskQueue.empty()) {
            // Skip and remove tasks that were flagged as removed
            if (mTaskQueue.top()->removed) {
                mTaskQueue.pop();
                continue;
            }

            // Break if the earliest task is not ready yet
            if (mTaskQueue.top()->nextRun > now) break;

            auto task = mTaskQueue.top();
            mTaskQueue.pop();

            // Advance to next occurrence before dispatching current one
            if (task->advanceToNext()) {
                mTaskQueue.push(task);
            }

            dispatchAction(task);
        }

        // Set timer for next occurrence
        scheduleNextTimer();
    }

    void Scheduler::dispatchAction(const TaskPtr &task) const {
        const auto &action = task->action;

        if (!action.contains(jrs::RequestKeys::METHOD) || !action[jrs::RequestKeys::METHOD].is_string()) {
            mpLogger->errorf("[SCHEDULER] Invalid action for device [%u]: missing or invalid method", task->deviceId);
            return;
        }

        if (!action.contains(jrs::RequestKeys::PARAMS) || !action[jrs::RequestKeys::PARAMS].is_object()) {
            mpLogger->errorf("[SCHEDULER] Invalid action for device [%u]: missing or invalid params", task->deviceId);
            return;
        }

        const auto &method = action[jrs::RequestKeys::METHOD].get<std::string>();
        const auto &params = action[jrs::RequestKeys::PARAMS];

        std::pair<std::string, std::string> parsedTargetMethod;
        try {
            parsedTargetMethod = API::parseTargetMethodString(method);
        } catch (const std::exception &e) {
            mpLogger->errorf("[SCHEDULER] Invalid method format in action for device [%u]: %s", task->deviceId,
                             e.what());
            return;
        }

        // Build InternalApi Command and Request
        API::InternalApi::Command command(params,
                                          API::ApiId(API::getNextApiId()),
                                          API::InternalApi::Method(parsedTargetMethod.second),
                                          API::InternalApi::Target(parsedTargetMethod.first));


        API::InternalApi::Request request;
        request.connectionId = 0; // No real connection
        request.isResultStructured = true;
        request.commands.push_back(std::move(command));

        mpLogger->debugf("[SCHEDULER] Dispatching action '%s' for device [%u]",
                         parsedTargetMethod.second.c_str(), task->deviceId);

        Actions::handleIncomingRequest(
            request, [this, deviceId = task->deviceId](connectionId_t, const std::string &&response) {
                mpLogger->debugf("[SCHEDULER] Action result for device [%u]: %s", deviceId, response.c_str());

                try {
                    const API::ApiResponse apiResponse(nlohmann::json::parse(response));
                    if (apiResponse.error.has_value()) {
                        mpLogger->errorf("[SCHEDULER] Action error for device [%u]: %s",
                                         deviceId,
                                         apiResponse.error->data.c_str());

                        const auto deviceOpt = Core::Instance().configCache().getDevice(deviceId);
                        if (deviceOpt.has_value()) {
                            DatabaseActions::postLog(deviceOpt.value().moduleId,
                                                     "error",
                                                     "Scheduled action failed: " + apiResponse.error->data);
                        }
                    }
                } catch (const std::exception &e) {
                    mpLogger->errorf("[SCHEDULER] Failed to parse action response for device [%u]: %s", deviceId,
                                     e.what());
                }
            });
    }
}
