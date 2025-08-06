#include "standalone_service.h"

#include <iostream>

namespace SmartHome::Service {
    StandaloneService::StandaloneService(const std::shared_ptr<Utils::Logger> &logger): ServiceManager(logger) {
    }

    bool StandaloneService::onInitialize() {
        try {
            lockFile.emplace(ms_LOCK_FILE_PATH);
            return true;
        } catch (std::exception &e) {
            mpLogger->errorf("[STANDALONE_SERVICE] Service initialization error: %s", e.what());
            return false;
        }
    }

    void StandaloneService::onStart() {
        mpLogger->infof("[STANDALONE_SERVICE] Service started with PID: %d", getpid());
    }

    void StandaloneService::onStop() {
        mpLogger->debug("[STANDALONE_SERVICE] Stopping service");
        //TODO add timeout
        lockFile.reset();
    }
}
