#include "socket_server.h"

#include <filesystem>
#include <iostream>
#include <ranges>

namespace bs = boost::system;

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
                                              const std::shared_ptr<Utils::Logger> &logger) {
        mConfig = config;
        bool isIpcLaunchSuccessful = false;
        mpLogger = logger;

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
                                       ba::io_context *ioContext,
                                       const bs::error_code &ec,
                                       const SocketConnection::Type type) {
        if (!ec && mIsSocketServerRunning.load()) {
            onAccept(connection, ioContext);
            if (type == SocketConnection::Type::TCP) {
                startTcpAcceptor(ioContext);
            } else {
                startUdsAcceptor(ioContext);
            }
        } else if (ec) {
            onAcceptError(ec);
        }
    };


    void SocketServer::startTcpAcceptor(ba::io_context *ioContext) {
        auto newTcpConnection = std::make_shared<SocketServerConnection>(
            *ioContext, SocketServerConnection::Type::TCP, mpLogger);
        auto &socket = std::get<bai::tcp::socket>(newTcpConnection->mSocket);

        mpTcpAcceptor->async_accept(socket, [this, newTcpConnection, ioContext](const bs::error_code ec) {
            acceptorHandler(newTcpConnection, ioContext, ec, SocketConnection::Type::TCP);
        });
    }

    void SocketServer::startUdsAcceptor(ba::io_context *ioContext) {
        auto newUdsConnection = std::make_shared<SocketServerConnection>(
            *ioContext, SocketServerConnection::Type::UDS, mpLogger);
        auto &socket = std::get<bal::stream_protocol::socket>(newUdsConnection->mSocket);

        mpUdsAcceptor->async_accept(socket, [this, newUdsConnection, ioContext](const bs::error_code ec) {
            acceptorHandler(newUdsConnection, ioContext, ec, SocketConnection::Type::UDS);
        });
    }

    void SocketServer::onAccept(const std::shared_ptr<SocketServerConnection> &connection, ba::io_context *ioContext) {
        // Try fetching unused connection id
        uint32_t connectionId;
        try {
            connectionId = getNextConnectionId();
        } catch (std::exception &e) {
            mpLogger->errorf("[SOCKET_SERVER] Socket server accept failed: %s", e.what());
            connection->close();
            // Retry after dealy
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return;
        }

        // TODO add active connections limit
        // New connection setup
        connection->setId(connectionId);
        connection->setCloseCallback([this](const uint32_t id) {
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
        connection->asyncReadLoop([this, connection, connectionId](const std::string &message) {
            mpLogger->debugf("[SOCKET_SERVER] Message received: %s", message.c_str());

            //TODO implement api integration and change line below for an api call
            connection->writeAsync(message, [message, this]() {
                mpLogger->debugf("[SOCKET_SERVER] Message sent: %s", message.c_str());
            });
        });
    }

    uint32_t SocketServer::getNextConnectionId() {
        std::lock_guard lock(mActiveConnectionsMutex);

        // TODO consider using another method then iteration from 1
        for (uint32_t id = 1; id < UINT32_MAX; ++id) {
            if (!mActiveConnections.contains(id)) {
                return id;
            }
        }

        throw std::runtime_error("Could not find free connection id");
    }

    void SocketServer::removeActiveConnection(const uint32_t connectionId) {
        std::lock_guard lock(mActiveConnectionsMutex);
        mActiveConnections.erase(connectionId);
    }

    void SocketServer::runSocketServer(ba::io_context *ioContext) {
        // Start accepting connections if socket server is initialized
        if (mIsSocketServerInitialized) {
            mIsSocketServerRunning.store(true);
            if (mpTcpAcceptor) startTcpAcceptor(ioContext);
            if (mpUdsAcceptor) startUdsAcceptor(ioContext);
        } else {
            mpLogger->errorf("[SOCKET_SERVER] IPC server not initialized");
        }
    }

    void SocketServer::stopSocketServer() {
        mIsSocketServerRunning.store(false);

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

        // Make a vector of all active connection
        std::vector<std::shared_ptr<SocketServerConnection> > connectionsToClose;
        {
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

    bool SocketServer::isRunning() const {
        return mIsSocketServerRunning.load();
    }
}
