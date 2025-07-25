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

    bool SocketServer::initializeSocketServer(ba::io_context *ioContext, const Config &config) {
        mConfig = config;
        bool isIpcLaunchSuccessful = false;

        // Create endpoint and acceptor for TCP connections if enabled
        if (mConfig.tcp.isEnabled) {
            try {
                mTcpEndpoint = bai::tcp::endpoint(bai::make_address(mConfig.tcp.endpointAddress),
                                                  mConfig.tcp.endpointPort);
                mpTcpAcceptor = std::make_unique<bai::tcp::acceptor>(*ioContext, mTcpEndpoint);
                std::cout << "IPC TCP acceptor started on " << mConfig.tcp.endpointAddress << ":"
                        << mConfig.tcp.endpointPort << std::endl;
                isIpcLaunchSuccessful = true;
            } catch (std::exception &e) {
                std::cerr << "Error during TCP acceptor start: " << e.what() << std::endl;
            }
        }

        // Create endpoint and acceptor for UDS connections if enabled
        if (mConfig.uds.isEnabled) {
            try {
                std::filesystem::remove(mConfig.uds.endpointPath);
                mUdsEndpoint = bal::stream_protocol::endpoint(mConfig.uds.endpointPath);
                mpUdsAcceptor = std::make_unique<bal::stream_protocol::acceptor>(*ioContext, mUdsEndpoint);
                std::cout << "IPC UDS acceptor started on " << mConfig.uds.endpointPath << std::endl;
                isIpcLaunchSuccessful = true;
            } catch (std::exception &e) {
                std::cerr << "Error during UDS acceptor start: " << e.what() << std::endl;
            }
        }

        mIsSocketServerInitialized.store(isIpcLaunchSuccessful);
        return isIpcLaunchSuccessful;
    }

    void SocketServer::onAcceptError(const bs::error_code &ec) {
        // TODO more cases or if?
        switch (ec.value()) {
            case ba::error::operation_aborted:
                std::cout << "IPC accept connection aborted" << std::endl;
                break;
            default:
                std::cerr << "Accept connection error: " << ec.message() << std::endl;
        }
    }

    void SocketServer::acceptorHandler(const std::shared_ptr<SocketServerConnection> &connection,
                                       ba::io_context *ioContext, const bs::error_code &ec,
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
        auto newTcpConnection = std::make_shared<SocketServerConnection>(*ioContext, SocketServerConnection::Type::TCP);
        auto &socket = std::get<bai::tcp::socket>(newTcpConnection->mSocket);

        mpTcpAcceptor->async_accept(socket, [this, newTcpConnection, ioContext](const bs::error_code ec) {
            acceptorHandler(newTcpConnection, ioContext, ec, SocketConnection::Type::TCP);
        });
    }

    void SocketServer::startUdsAcceptor(ba::io_context *ioContext) {
        auto newUdsConnection = std::make_shared<SocketServerConnection>(*ioContext, SocketServerConnection::Type::UDS);
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
            std::cerr << "Socket server accept connection error: " << e.what() << std::endl;
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

        std::visit([connectionId](auto &socket) {
            std::cout << "Connection [" << connectionId << "] accepted" << std::endl;
        }, connection->mSocket);

        // Start reading incoming messages
        connection->asyncReadLoop([this, connection, connectionId](const std::string &message) {
            std::cout << "Received from" << connectionId << ": " << message << std::endl;

            //TODO implement api integration and change line below for an api call
            connection->writeAsync(message, [message]() { std::cout << "Sent message: " << message << std::endl; });
        });
    }


    uint32_t SocketServer::getNextConnectionId() {
        std::lock_guard lock(mActiveConnectionsMutex);

        // TODO consider using another method then iteration from 1
        for (uint32_t id = 1; id < std::numeric_limits<uint32_t>::max(); ++id) {
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
            std::cerr << "IPC server run error: server not initialized" << std::endl;
        }
    }

    void SocketServer::stopSocketServer() {
        mIsSocketServerRunning.store(false);

        // Close TCP acceptor
        if (mpTcpAcceptor) {
            std::cout << "Stopping TCP acceptor" << std::endl;
            try {
                bs::error_code ec;
                mpTcpAcceptor->cancel(ec);
                mpTcpAcceptor->close(ec);
                if (ec) {
                    std::cerr << "Error during server close: " << ec.message() << std::endl;
                }
            } catch (std::exception &e) {
                std::cerr << "Unexpected error during server close: " << e.what() << std::endl;
            }
            mpTcpAcceptor.reset();
        }

        // Close UDS acceptor
        if (mpUdsAcceptor) {
            std::cout << "Stopping UDS acceptor" << std::endl;
            try {
                bs::error_code ec;
                mpUdsAcceptor->cancel(ec);
                mpUdsAcceptor->close(ec);
                if (ec) {
                    std::cerr << "Error during server close: " << ec.message() << std::endl;
                }
            } catch (std::exception &e) {
                std::cerr << "Unexpected error during server close: " << e.what() << std::endl;
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
