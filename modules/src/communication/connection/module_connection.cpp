#include "module_connection.h"

ModuleConnection *ModuleConnection::mspConnection = nullptr;

ModuleConnection::ModuleConnection(Communication *communication, const std::shared_ptr<ul::Logger> &logger)
    : Connection(communication, logger) {
    mspConnection = this;
    createConnectionTask(connectionTask);

    mpLogger->info("ModuleConnection Class", "ModuleConnection initialized.");
}

ModuleConnection::~ModuleConnection() {
    deleteConnectionTask();
}

void ModuleConnection::connectionTask(void* parameters) {
    const auto &con = *mspConnection;

    bool isConnected = false;
    uint8_t ackNumber = 1;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}