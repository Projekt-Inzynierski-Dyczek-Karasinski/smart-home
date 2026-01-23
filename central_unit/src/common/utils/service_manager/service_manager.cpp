#include "service_manager.h"
#include "standalone_service.h"
#include "systemd_service.h"

namespace SmartHome::Utils {
    ServiceManager::ServiceManager(const std::shared_ptr<Logger> &logger,
                                   const std::string_view serviceName) : mpLogger(logger), mServiceName(serviceName) {
    }

    ServiceManager::~ServiceManager() = default;

    std::unique_ptr<ServiceManager> ServiceManager::create(const std::shared_ptr<Logger> &logger,
                                                           std::string_view serviceName,
                                                           const ServiceType type) {
        switch (type) {
            case ServiceType::AUTO: {
                if (getenv("NOTIFY_SOCKET")) {
                    return std::make_unique<SystemdService>(logger,serviceName);
                }
                return std::make_unique<StandaloneService>(logger, serviceName);
            }

            case ServiceType::SYSTEMD:
                return std::make_unique<SystemdService>(logger, serviceName);

            default:
            case ServiceType::STANDALONE:
                return std::make_unique<StandaloneService>(logger, serviceName);
        }
    }

    void ServiceManager::setIoContext(ba::io_context &io_context) {
        mpIoContext = &io_context;
    }
}
