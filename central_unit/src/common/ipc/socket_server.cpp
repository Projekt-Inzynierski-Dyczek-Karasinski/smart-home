#include "socket_server.h"

#include <filesystem>
#include <iostream>
#include <ranges>

namespace bs = boost::system;
using namespace std::chrono_literals;

namespace SmartHome::IPC {
    SocketServer &SocketServer::Instance() {
        static SocketServer ServerInstance;
        return ServerInstance;
    }

    SocketServer::SocketServer() = default;

    SocketServer::~SocketServer() {
        if (mIsSocketServerRunning) {
            stopSocketServer();
        }
    }

    bool SocketServer::initializeSocketServer(ba::io_context *ioContext,
                                              const Config &config,
                                              const std::shared_ptr<API::Api> &api,
                                              const std::shared_ptr<Utils::Logger> &logger) {
        mpIoContext = ioContext;
        mConfig = config;
        bool isIpcLaunchSuccessful = false;
        mpLogger = logger;
        mpApi = api;

        // Create endpoint and acceptor for TCP connections if enabled
        if (mConfig.tcp.isEnabled) {
            try {
                mTcpEndpoint = bai::tcp::endpoint(bai::make_address(mConfig.tcp.endpointAddress),
                                                  mConfig.tcp.endpointPort);
                mpTcpAcceptor = std::make_unique<bai::tcp::acceptor>(*ioContext, mTcpEndpoint);
                mpLogger->infof("[SOCKET_SERVER] TCP acceptor started on %s:%d", mConfig.tcp.endpointAddress.c_str(),
                                mConfig.tcp.endpointPort);
                isIpcLaunchSuccessful = true;
            } catch (bs::system_error &e) {
                mpLogger->errorf("[SOCKET_SERVER] TCP acceptor start failed: %s (category: %s ,code: %d)",
                                 e.what(),
                                 e.code().category().name(),
                                 e.code().value());
            } catch (std::filesystem::filesystem_error &e) {
                mpLogger->errorf("[SOCKET_SERVER] TCP acceptor start failed %s", e.what());
            } catch (std::exception &e) {
                mpLogger->errorf("[SOCKET_SERVER] TCP acceptor start failed unexpected error: %s", e.what());
            }
        }

        // Create endpoint and acceptor for UDS connections if enabled
        if (mConfig.uds.isEnabled) {
            try {
                std::filesystem::remove(mConfig.uds.endpointPath);
                mUdsEndpoint = bal::stream_protocol::endpoint(mConfig.uds.endpointPath);
                mpUdsAcceptor = std::make_unique<bal::stream_protocol::acceptor>(*ioContext, mUdsEndpoint);
                mpLogger->infof("[SOCKET_SERVER] UDS acceptor started on %s", mConfig.uds.endpointPath.c_str());
                isIpcLaunchSuccessful = true;
            } catch (bs::system_error &e) {
                mpLogger->errorf("[SOCKET_SERVER] UDS acceptor start failed: %s (category: %s ,code: %d)",
                                 e.what(),
                                 e.code().category().name(),
                                 e.code().value());
            } catch (std::filesystem::filesystem_error &e) {
                mpLogger->errorf("[SOCKET_SERVER] UDS acceptor start failed %s", e.what());
            } catch (std::exception &e) {
                mpLogger->errorf("[SOCKET_SERVER] UDS acceptor start failed unexpected error: %s", e.what());
            }
        }

        mIsSocketServerInitialized.store(isIpcLaunchSuccessful);
        return isIpcLaunchSuccessful;
    }

    void SocketServer::onAcceptError(const bs::error_code &ec) const {
        // TODO more cases or if?
        switch (ec.value()) {
            case ba::error::operation_aborted:
                mpLogger->info("[SOCKET_SERVER] IPC accept connection aborted");
                break;
            default:
                mpLogger->errorf("[SOCKET_SERVER] IPC accept failed: %s", ec.message().c_str());
        }
    }

    void SocketServer::acceptorHandler(const std::shared_ptr<SocketServerConnection> &connection,
                                       const bs::error_code &ec,
                                       const SocketConnection::Type type) {
        if (!ec && mIsSocketServerRunning.load() && mIsAcceptingNewConnections.load()) {
            onAccept(connection);
            if (type == SocketConnection::Type::TCP) {
                startTcpAcceptor();
            } else {
                startUdsAcceptor();
            }
        } else if (ec) {
            onAcceptError(ec);
        }
    }


    void SocketServer::startTcpAcceptor() {
        auto newTcpConnection = std::make_shared<SocketServerConnection>(
            *mpIoContext, SocketServerConnection::Type::TCP, mpLogger);
        auto &socket = std::get<bai::tcp::socket>(newTcpConnection->mSocket);

        mIsAcceptingNewConnections.store(true);
        mpTcpAcceptor->async_accept(socket, [this, newTcpConnection](const bs::error_code ec) {
            acceptorHandler(newTcpConnection, ec, SocketConnection::Type::TCP);
        });
    }

    void SocketServer::startUdsAcceptor() {
        auto newUdsConnection = std::make_shared<SocketServerConnection>(
            *mpIoContext, SocketServerConnection::Type::UDS, mpLogger);
        auto &socket = std::get<bal::stream_protocol::socket>(newUdsConnection->mSocket);

        mIsAcceptingNewConnections.store(true);
        mpUdsAcceptor->async_accept(socket, [this, newUdsConnection](const bs::error_code ec) {
            acceptorHandler(newUdsConnection, ec, SocketConnection::Type::UDS);
        });
    }

    void SocketServer::onAccept(const std::shared_ptr<SocketServerConnection> &connection) {
        // Try fetching unused connection id
        connectionId_t connectionId;
        try {
            connectionId = getNextConnectionId();
        } catch (std::exception &e) {
            mpLogger->errorf("[SOCKET_SERVER] Socket server accept failed: %s", e.what());
            connection->close();
            // Retry after dealy
            std::this_thread::sleep_for(1s);
            return;
        }

        // TODO add active connections limit
        // New connection setup
        connection->setId(connectionId);
        connection->setCloseCallback([this](const connectionId_t id) {
            removeActiveConnection(id);
        });

        // Save new connection to active connections map
        {
            std::lock_guard lock(mActiveConnectionsMutex);
            mActiveConnections[connectionId] = connection;
        }

        std::visit([connectionId, this](auto &socket) {
            mpLogger->debugf("[SOCKET_SERVER] Connection accepted (ID:%d)", connectionId);
        }, connection->mSocket);

        // Start reading incoming messages
        connection->asyncReadLoop([this, connectionId](const std::string &message) {
            mpLogger->debugf("[SOCKET_SERVER] Message received: %s", message.c_str());
            mpApi->handleRequest(connectionId, message.data());
        });
    }

    connectionId_t SocketServer::getNextConnectionId() {
        std::lock_guard lock(mActiveConnectionsMutex);

        // TODO consider using another method then iteration from 1
        for (connectionId_t id = 1; id < std::numeric_limits<decltype(id)>::max(); ++id) {
            if (!mActiveConnections.contains(id)) {
                return id;
            }
        }

        throw std::runtime_error("Could not find free connection id");
    }

    void SocketServer::removeActiveConnection(const connectionId_t connectionId) {
        std::lock_guard lock(mActiveConnectionsMutex);
        mActiveConnections.erase(connectionId);
    }

    void SocketServer::runSocketServer() {
        // Start accepting connections if socket server is initialized
        if (mIsSocketServerInitialized) {
            mIsSocketServerRunning.store(true);
            if (mpTcpAcceptor) startTcpAcceptor();
            if (mpUdsAcceptor) startUdsAcceptor();
        } else {
            mpLogger->errorf("[SOCKET_SERVER] IPC server not initialized");
        }
    }

    void SocketServer::stopSocketServer() {
        mIsSocketServerRunning.store(false);

        if (mIsAcceptingNewConnections) {
            stopAcceptors();
        }
        stopIncomingTraffic();

        // Make a vector of all active connection
        std::vector<std::shared_ptr<SocketServerConnection> > connectionsToClose; {
            std::lock_guard lock(mActiveConnectionsMutex);

            for (auto &weakConnection: mActiveConnections | std::views::values) {
                if (auto connection = weakConnection.lock()) {
                    connectionsToClose.push_back(connection);
                }
            }
            mActiveConnections.clear();
        }

        // Close all active connections
        for (const auto &connection: connectionsToClose) {
            connection->close();
        }
    }

    void SocketServer::stopAcceptors() {
        // Close TCP acceptor
        if (mpTcpAcceptor) {
            mpLogger->debug("[SOCKET_SERVER] Stopping TCP acceptor");
            try {
                bs::error_code ec;
                mpTcpAcceptor->cancel(ec);
                mpTcpAcceptor->close(ec);
                if (ec) {
                    mpLogger->debugf("[SOCKET_SERVER] TCP acceptor close failed: %s", ec.message().c_str());
                }
            } catch (std::exception &e) {
                mpLogger->errorf("[SOCKET_SERVER] TCP acceptor close unexpected error: %s", e.what());
            }
            mpTcpAcceptor.reset();
        }

        // Close UDS acceptor
        if (mpUdsAcceptor) {
            mpLogger->debug("[SOCKET_SERVER] Stopping UDS acceptor");
            try {
                bs::error_code ec;
                mpUdsAcceptor->cancel(ec);
                mpUdsAcceptor->close(ec);
                if (ec) {
                    mpLogger->debugf("[SOCKET_SERVER] UDS acceptor close failed: %s", ec.message().c_str());
                }
            } catch (std::exception &e) {
                mpLogger->errorf("[SOCKET_SERVER] UDS acceptor close unexpected error: %s", e.what());
            }
            mpUdsAcceptor.reset();
        }

        mIsAcceptingNewConnections.store(false);
    }

    void SocketServer::stopIncomingTraffic() {
        std::scoped_lock lock(mActiveConnectionsMutex);
        for (auto &weakConnection: mActiveConnections | std::views::values) {
            if (auto connection = weakConnection.lock()) {
                connection->shutdownSocket(ba::socket_base::shutdown_receive);
                //TODO Sockets do not close for incoming traffic consistently - more testing needed
            }
        }
    }

    bool SocketServer::isRunning() const {
        return mIsSocketServerRunning.load();
    }

    std::shared_ptr<SocketServerConnection> SocketServer::getConnection(const connectionId_t connectionId) {
        std::scoped_lock lock(mActiveConnectionsMutex);
        auto iter = mActiveConnections.find(connectionId);
        if (iter == mActiveConnections.end()) {
            return nullptr;
        }
        return std::shared_ptr(iter->second);
    }

    ba::io_context *SocketServer::getIoContext() const {
        return mpIoContext;
    }
}
