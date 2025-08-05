#pragma once

#include "service_manager.h"
#include "../core.h"

#include <optional>

#include <boost/asio.hpp>

namespace ba = boost::asio;

namespace SmartHome::Service {
    /**
     * @brief Systemd service implementation.
     *
     * @details Integrates with systemd service manager providing:
     *              - Status notifications
     *              - Watchdog support with automatic keep-alive
     *
     * @note Requires compilation with WITH_SYSTEMD flag.
     */
    class SystemdService final : public ServiceManager {
    public:
        explicit SystemdService(const std::shared_ptr<Utils::Logger> &logger);

        bool onInitialize() override;

        void onStart() override;

        void onStop() override;

    private:
        /**
         * @brief Sends periodic watchdog notifications to systemd.
         *
         * @details Automatically reschedules itself at half the watchdog interval. Stops when Core is shutting down.
         */
        void notifyWatchdog();

        Core &mCore = Core::Instance();

        // Watchdog variables
        std::optional<ba::steady_timer> mWatchdogTimer; ///< Watchdog timer for periodic notifications
        uint64_t mWatchdogInterval = 0; ///< Watchdog interval in microseconds
        bool mIsWatchdogEnabled = false;
    };
}
