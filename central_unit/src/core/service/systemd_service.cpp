#ifdef WITH_SYSTEMD
#include "systemd_service.h"
#include "../core.h"

#include <iostream>

#include <systemd/sd-daemon.h>

namespace SmartHome::Service {
    SystemdService::SystemdService(const std::shared_ptr<Utils::Logger> &logger): ServiceManager(logger) {
    }

    bool SystemdService::onInitialize() {
        if (getenv("NOTIFY_SOCKET")) {
            mIsWatchdogEnabled = sd_watchdog_enabled(0, &mWatchdogInterval) > 0;
            if (mIsWatchdogEnabled) {
                mWatchdogTimer.emplace(Core::Instance().getCoreUtilityIoContext());
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
                const auto &core = Core::Instance();
                // Reschedule if Core is still running
                if (!ec && core.isRunning()) {
                    sd_notify(0, "WATCHDOG=1");
                    notifyWatchdog();
                }
            });
        }
    }
}
#endif
