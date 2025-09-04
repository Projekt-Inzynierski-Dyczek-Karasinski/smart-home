#include "module_connection.h"

namespace Comms {
    ModuleConnection::ModuleConnection(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : Connection(communication, logger) {

        // createConnectionTask(connectionTask);

        mpLogger->info("ModuleConnection Class", "ModuleConnection initialized.");
    }

    ModuleConnection::~ModuleConnection() {

    }

    void ModuleConnection::connectionTask(void* parameters) {
        const auto &con = *mspConnection;

        bool isConnected = false;

        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));

        }
    }
}