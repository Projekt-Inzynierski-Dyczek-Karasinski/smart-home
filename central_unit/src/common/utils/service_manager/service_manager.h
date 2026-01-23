#pragma once

#include "utils.h"
#include "async_logger.h"

#include <memory>

namespace SmartHome::Utils {
    /**
     * @brief Abstract base class for  service lifecycle management.
     *
     * @details Provides interface for different service implementations.
     *          Implementations must handle platform-specific service management.
     */
    class ServiceManager {
    public:
        /**
         * @brief Construct ServiceManage.
         *
         * @param logger Shared pointer instance reference of logger.
         * @param serviceName Service name used for LockFile.
         */
        explicit ServiceManager(const std::shared_ptr<Logger> &logger, std::string_view serviceName);

        virtual ~ServiceManager();

        /**
         * @brief Factory  method to create appropriate service manager.
         *
         * @details Creates service manager based on type and/or runtime environment.
         *          AUTO mode detects systemd presence via NOTIFY_SOCKET environment variable.
         *
         * @param logger Synchronous logger instance needed for initialization.
         * @param serviceName Service name used for LockFile.
         * @param type Service type to create manager for.
         * @return Unique pointer to created service manager instance.
         *
         * @note Falls back to StandaloneService if systemd is not available.
         */
        static std::unique_ptr<ServiceManager> create(const std::shared_ptr<Logger> &logger,
                                                      std::string_view serviceName,
                                                      ServiceType type = ServiceType::AUTO);

        /**
         * @brief Initialize service infrastructure.
         *
         * @details Called once before service starts. Sets up platform-specific requirements.
         *
         * @return  true if initialization successful, false otherwise.
         */
        virtual bool onInitialize() = 0;

        /**
         * @brief Start service operation.
         *
         * @details Called after successful initialization. Performs platform-specific service functions.
         *
         * @pre onInitialize() must return true.
         */
        virtual void onStart() = 0;

        /**
         * @brief Stops service and performs cleanup.
         *
         * @details Part of gracefully shutdown process. Handles platform-specific cleanup and service shutdown.
         */
        virtual void onStop() = 0;

        /**
         * @brief Set IO context for async operations.
         *
         * @param io_context IO context reference for watchdog timers and async operations.
         *
         * @note Required for SystemdService watchdog functionality.
         *       Should be called  before onInitialize() for services that need watchdog support.
         */
        void setIoContext(ba::io_context &io_context);

    protected:
        /// Path to lock file
        static constexpr std::string_view ms_LOCK_FILE_PATH = "/var/lock/";

        std::shared_ptr<Logger> mpLogger; ///< Logger instance shared pointer
        ba::io_context *mpIoContext{}; ///< Used in systemd service for watchdog timers
        std::string mServiceName; ///< Used as LockFile name
        std::optional<FileLock> lockFile; ///< Lock file RAII wrapper instance
    };
}
