#ifdef WITH_SYSTEMD
#include "systemd_service.h"

#include <systemd/sd-daemon.h>

namespace SmartHome::Utils {
    SystemdService::SystemdService(const std::shared_ptr<Logger> &logger,
                                   const std::string_view serviceName) : ServiceManager(logger, serviceName) {
    }

    bool SystemdService::onInitialize() {
        try {
            lockFile.emplace(ms_LOCK_FILE_PATH.data() + mServiceName + ".lock");
        } catch (std::exception &e) {
            mpLogger->errorf("[SYSTEMD_SERVICE] Service initialization error: %s", e.what());
            return false;
        }

        if (getenv("NOTIFY_SOCKET")) {
            if (mpIoContext == nullptr) {
                mpLogger->error(
                    "[SYSTEMD_SERVICE] Watchdog is enabled but IoContext was not set. "
                    "Use ServiceManager.setIoContext(boost::asio::io_context&)");
                return false;
            }
            mIsWatchdogEnabled = sd_watchdog_enabled(0, &mWatchdogInterval) > 0;
            if (mIsWatchdogEnabled) {
                mWatchdogTimer.emplace(*mpIoContext);
            }
            return true;
        }
        mpLogger->error("[SYSTEMD_SERVICE] Service initialization failed, could not find notify_socket");
        return false;
    }

    void SystemdService::onStart() {
        sd_notify(0, "READY=1");
        if (mIsWatchdogEnabled) {
            sd_notify(0, "WATCHDOG=1");
            notifyWatchdog();
        }
        mpLogger->info("[SYSTEMD_SERVICE] Systemd service started");
    }

    void SystemdService::onStop() {
        mpLogger->debug("[SYSTEMD_SERVICE] Stopping service");
        sd_notify(0, "STOPPING=1");
        if (mIsWatchdogEnabled && mWatchdogTimer.has_value()) {
            mWatchdogTimer->cancel();
            mWatchdogTimer.reset();
        }
    }

    void SystemdService::notifyWatchdog() {
        if (mWatchdogTimer.has_value()) {
            auto &timer = mWatchdogTimer.value();
            // Schedule at half interval value
            timer.expires_after(std::chrono::microseconds(mWatchdogInterval) / 2);
            timer.async_wait([this](const boost::system::error_code &ec) {
                // Reschedule if no errors are present
                if (!ec) {
                    sd_notify(0, "WATCHDOG=1");
                    notifyWatchdog();
                }
            });
        }
    }
}
#endif
