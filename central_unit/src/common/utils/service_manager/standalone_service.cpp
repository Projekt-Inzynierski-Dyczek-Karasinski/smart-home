#include "standalone_service.h"

namespace SmartHome::Utils {
    StandaloneService::StandaloneService(const std::shared_ptr<Logger> &logger, const std::string_view serviceName) : ServiceManager(logger, serviceName) {
    }

    bool StandaloneService::onInitialize() {
        try {
            lockFile.emplace(ms_LOCK_FILE_PATH.data() + mServiceName + ".lock");
            return true;
        } catch (std::exception &e) {
            mpLogger->errorf("[STANDALONE_SERVICE] Service initialization error: %s", e.what());
            return false;
        }
    }

    void StandaloneService::onStart() {
        mpLogger->infof("[STANDALONE_SERVICE] Service (%s) started with PID: %d",
                        mServiceName.c_str(), getpid());
    }

    void StandaloneService::onStop() {
        mpLogger->infof("[STANDALONE_SERVICE] Stopping service (%s) PID: %d",
                        mServiceName.c_str(), getpid());
        lockFile.reset();
    }
}
