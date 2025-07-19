#pragma once

#include <optional>

#include "service_manager.h"
#include "utils.h"

namespace SmartHome::Service {
    /**
     * @brief Standalone service implementation.
     *
     * @details Ensures single instance via exclusive file lock. Lock is automatically released on service stop or crash.
     */
    class StandaloneService final : public ServiceManager {
    public:
        bool onInitialize() override;

        void onStart() override;

        void onStop() override;

    private:
        /// Path to lock file
        static constexpr const char *lockFilePath = "/var/lock/smarthomed.lock";
        /// Lock file RAII wrapper instance
        std::optional<Utils::FileLock> lockFile;
    };
}
