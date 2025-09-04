#include "central_unit_connection.h"

namespace Comms {
    CentralUnitConnection::CentralUnitConnection(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
        : Connection(communication, logger) {
        // createConnectionTask(connectionTask);

        mpLogger->info("CentralUnitConnection Class", "CentralUnitConnection initialized.");
    }

    CentralUnitConnection::~CentralUnitConnection() {
    }

    void CentralUnitConnection::connectionTask(void* parameters) {
        const auto &con = *mspConnection;

        bool isConnected = false;
        uint8_t ackNumber = 1;

        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}