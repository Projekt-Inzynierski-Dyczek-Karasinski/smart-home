#pragma once

#include "socket_connection.h"
#include "async_logger.h"

#include <string_view>

namespace si = SmartHome::IPC;
namespace su = SmartHome::Utils;

namespace SmartHomeMediator {
    class ApiClient {
    public:
        explicit ApiClient(ba::io_context *io_context, const std::shared_ptr<su::Logger> &logger);

        ~ApiClient();

        bool connectToServer(std::string_view udsPath);

        bool connectToServer(std::string_view ipAddress, int port);

        void startReceiving(const std::function<void(const std::string &message)> &handleMessage);

        void send(std::string_view message);

    private:
        ba::io_context *mpIoContext;
        std::shared_ptr<su::Logger> mpLogger;
        std::optional<si::SocketConnection> mConnection;
    };
}
