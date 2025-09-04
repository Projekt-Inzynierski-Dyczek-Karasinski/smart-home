#pragma once

#include "connection.h"

namespace Comms {
    class CentralUnitConnection final : public Connection {
    public:
        CentralUnitConnection(Communication *communication, const std::shared_ptr<ul::Logger> &logger);
        ~CentralUnitConnection() override;

    private:
        static void connectionTask(void* parameters);


    };
}