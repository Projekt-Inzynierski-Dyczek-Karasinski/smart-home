#pragma once

#include "service_manager.h"

#include <optional>

#include <boost/asio.hpp>

namespace ba = boost::asio;

namespace SmartHome::Utils {
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
        /**
         * @brief Construct systemd service manager.
         *
         * @param logger Shared logger instance for service messages.
         * @param serviceName Service name used for LockFile.
         */
        explicit SystemdService(const std::shared_ptr<Logger> &logger, std::string_view serviceName);

        /**
         * @brief Initialize systemd service.
         *
         * @details Checks for systemd environment (NOTIFY_SOCKET) and sets up watchdog timer if enabled.
         *          Watchdog interval is read from systemd configuration.
         *
         * @return true if systemd environment found and initialized,
         *         false if not running under systemd.
         */
        bool onInitialize() override;

        /**
         * @brief Start systemd service.
         *
         * @details Notifies systemd that service is ready and starts watchdog keep-alive timer if enabled.
         *
         * @pre onInitialize() called and returned true.
         */
        void onStart() override;

        /**
         * @brief Stop systemd service.
         *
         * @details Notifies systemd about shutdown and cancels watchdog timer.
         *          Systemd will wait for service to fully stop before considering it terminated.
         */
        void onStop() override;

    private:
        /**
         * @brief Sends periodic watchdog notifications to systemd.
         *
         * @details Automatically reschedules itself at half the watchdog interval. Stops when Core is shutting down.
         */
        void notifyWatchdog();

        // Watchdog variables
        std::optional<ba::steady_timer> mWatchdogTimer; ///< Watchdog timer for periodic notifications
        uint64_t mWatchdogInterval = 0; ///< Watchdog interval in microseconds
        bool mIsWatchdogEnabled = false;
    };
}
