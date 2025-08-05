#include "service_manager.h"
#include "standalone_service.h"
#ifdef WITH_SYSTEMD
#include "systemd_service.h"
#endif

namespace SmartHome::Service {
    ServiceManager::ServiceManager(const std::shared_ptr<Utils::Logger> &logger): mpLogger(logger) {
    }

    std::unique_ptr<ServiceManager> ServiceManager::create(const std::shared_ptr<Utils::Logger> &logger,
                                                           const Utils::ServiceType type) {
        switch (type) {
            case Utils::ServiceType::AUTO: {
                if (getenv("NOTIFY_SOCKET")) {
                    return std::make_unique<SystemdService>(logger);
                }
                return std::make_unique<StandaloneService>(logger);
            }

            case Utils::ServiceType::SYSTEMD:
                return std::make_unique<SystemdService>(logger);

            default:
            case Utils::ServiceType::STANDALONE:
                return std::make_unique<StandaloneService>(logger);
        }
    }
}
