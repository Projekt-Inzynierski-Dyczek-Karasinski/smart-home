#include "standalone_service.h"

#include <iostream>

namespace SmartHome::Service {
    bool StandaloneService::onInitialize() {
        try {
            lockFile.emplace(lockFilePath);
            return true;
        } catch (std::exception &e) {
            std::cerr << "Standalone service initialization error: " << e.what() << std::endl;
            return false;
        }
    }

    void StandaloneService::onStart() {
        std::cout << "Standalone service started with PID: " << getpid() << std::endl;
    }

    void StandaloneService::onStop() {
        std::cout << "Stopping standalone service" << std::endl;
        //TODO add timeout
        lockFile.reset();
    }
}
