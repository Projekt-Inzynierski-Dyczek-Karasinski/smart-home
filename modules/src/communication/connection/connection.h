#pragma once

#include <memory>

#include "utils/logger.h"
#include "utils/uint8_array_handlers.h"

namespace ul = Utils::Logging;
namespace uah = Utils::ArrayHandlers;

class Communication;

class Connection {
public:
    Connection(Communication *communication, const std::shared_ptr<ul::Logger> &logger);

    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    virtual ~Connection() = default;

protected:
    void sendConnectionRequest(uint8_t *ackNumber) const;

    void createConnectionQueues();
    void deleteConnectionQueues();

    void createConnectionTask(TaskFunction_t task);
    void deleteConnectionTask();

    Communication *mpCommunication; ///< Pointer to the Communication class instance.
    std::shared_ptr<ul::Logger> mpLogger;

    TaskHandle_t mConnectionTaskHandle = nullptr;
};


