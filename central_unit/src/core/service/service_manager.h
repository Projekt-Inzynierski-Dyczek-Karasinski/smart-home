#pragma once

#include "utils.h"
#include "async_logger.h"

#include <memory>

namespace SmartHome::Service {
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
         */
        explicit ServiceManager(const std::shared_ptr<Utils::Logger> &logger);

        virtual ~ServiceManager() = default;

        /**
         * @brief Factory  method to create appropriate service manager.
         *
         * @details Creates service manager based on type and/or runtime environment.
         *          AUTO mode detects systemd presence via NOTIFY_SOCKET environment variable.
         *
         * @param logger Synchronous logger instance needed for initialization.
         * @param type Service type to create manager for.
         * @return Unique pointer to created service manager instance.
         *
         * @note Falls back to StandaloneService if systemd is not available.
         */
        static std::unique_ptr<ServiceManager> create(const std::shared_ptr<Utils::Logger> &logger,
                                                      Utils::ServiceType type = Utils::ServiceType::AUTO);

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
         * @pre onInitialize() must return ture.
         */
        virtual void onStart() = 0;

        /**
         * @brief Stops service and performs cleanup.
         *
         * @details Part of gracefully shutdown process. Handles platform-specific cleanup and service shutdown.
         */
        virtual void onStop() = 0;

    protected:
        std::shared_ptr<Utils::Logger> mpLogger; ///< Logger instance shared pointer
    };
}
