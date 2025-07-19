#ifdef WITH_SYSTEMD
#include "systemd_service.h"
#include "../core.h"

#include <iostream>

#include <systemd/sd-daemon.h>

namespace SmartHome::Service {
    bool SystemdService::onInitialize() {
        if (getenv("NOTIFY_SOCKET")) {
            mIsWatchdogEnabled = sd_watchdog_enabled(0, &mWatchdogInterval) > 0;
            if (mIsWatchdogEnabled) {
                mWatchdogTimer.emplace(mCore.getCoreIoContext());
            }
            return true;
        }
        std::cerr << "Systemd service error: Cannot find notify_socket" << std::endl;
        return false;
    }

    void SystemdService::onStart() {
        sd_notify(0, "READY=1");
        if (mIsWatchdogEnabled) {
            sd_notify(0, "WATCHDOG=1");
            notifyWatchdog();
        }
    }

    void SystemdService::onStop() {
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
        };
    }
}
#endif
