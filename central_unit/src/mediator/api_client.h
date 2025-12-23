#pragma once

#include "socket_connection.h"
#include "async_logger.h"
#include "api.h"

#include <string_view>


namespace si = SmartHome::IPC;
namespace su = SmartHome::Utils;
namespace sa = SmartHome::API;

namespace SmartHomeMediator {
    class ApiClient final : public sa::Api {
    public:
        explicit ApiClient(ba::io_context *io_context, const std::shared_ptr<su::Logger> &logger);

        ~ApiClient() override;

        bool connectToServer(std::string_view udsPath);

        bool connectToServer(std::string_view ipAddress, int port);

        void initialize(const std::function<void(const std::string &message)> &messageHandler);

        void handleOutgoing(SmartHome::connectionId_t connectionId, std::string &&message) override;

        void handleIncoming(SmartHome::connectionId_t connectionId, std::string &&message) override;

    private:
        bool handshake();

        void startReceiving();

        void send(std::string_view message);

        ba::io_context *mpIoContext;
        std::shared_ptr<su::Logger> mpLogger;
        std::optional<si::SocketConnection> mConnection;

        std::function<void(const std::string &message)> mMessageHandler;
    };
}
