#include "service_manager.h"
#include "standalone_service.h"
#ifdef WITH_SYSTEMD
#include "systemd_service.h"
#endif

namespace SmartHome::Service {
    std::unique_ptr<ServiceManager> ServiceManager::create(const Utils::ServiceType type) {
        switch (type) {
            case Utils::ServiceType::AUTO: {
                if (getenv("NOTIFY_SOCKET")) {
                    return std::make_unique<SystemdService>();
                }
                return std::make_unique<StandaloneService>();
            }

            case Utils::ServiceType::SYSTEMD:
                return std::make_unique<SystemdService>();

            default:
            case Utils::ServiceType::STANDALONE:
                return std::make_unique<StandaloneService>();
        }
    };
}
