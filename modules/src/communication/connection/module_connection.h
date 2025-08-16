#pragma once

#include "connection.h"

class ModuleConnection final : public Connection {
public:
    ModuleConnection(Communication *communication, const std::shared_ptr<ul::Logger> &logger);
    ~ModuleConnection() override;
private:
    static void connectionTask(void* parameters);

    static ModuleConnection *mspConnection;
};

