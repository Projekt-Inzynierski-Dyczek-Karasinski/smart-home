#pragma once

#include <optional>

#include "service_manager.h"
#include "utils.h"

namespace SmartHome::Utils {
    /**
     * @brief Standalone service implementation.
     *
     * @details Ensures single instance via exclusive file lock. Lock is automatically released on service stop or crash.
     */
    class StandaloneService final : public ServiceManager {
    public:
        /**
         * @brief Construct standalone service manager.
         *
         * @param logger Shared logger instance for service messages.
         * @param serviceName Service name used for LockFile.
         */
        explicit StandaloneService(const std::shared_ptr<Logger> &logger, std::string_view serviceName);

        /**
         * @brief Initialize standalone service.
         *
         * @details Creates exclusive lock file to ensure only one instance of the service is running.
         *          If lock file already exists or fails to lock, initialization fails.
         *
         * @return true if lock file was created successfully, false locking file fails.
         */
        bool onInitialize() override;

        /**
         * @brief Start standalone service.
         *
         * @details Logs service start with process ID.
         *
         * @pre onInitialize() called and returned true.
         */
        void onStart() override;

        /**
         * @brief Stop standalone service.
         *
         * @details Releases and removes lock file to allow future service instances.
         *          Lock is also automatically released if service crashes.
         *
         * @warning Lock file will not be removed if service crashes, may cause file permissions problems.
         */
        void onStop() override;
    };
}
