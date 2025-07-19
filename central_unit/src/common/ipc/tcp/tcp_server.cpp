#include "tcp_server.h"

#include <iostream>
#include <ranges>

namespace bs = boost::system;

namespace SmartHome::IPC {
    TcpServer &TcpServer::Instance() {
        static TcpServer ServerInstance;
        return ServerInstance;
    }

    TcpServer::TcpServer() = default;

    TcpServer::~TcpServer() {
        if (mIsTcpServerRunning.load()) {
            stopTcpServer();
        }
    }

    bool TcpServer::startTcpServer(ba::io_context *ioContext, const std::string &address, const unsigned short &port) {
        try {
            // Create endpoint and acceptor for TCP connections
            mEndpoint = bip::tcp::endpoint(bip::make_address(address), port);
            mpAcceptor = std::make_unique<bip::tcp::acceptor>(*ioContext, mEndpoint);

            mIsTcpServerInitialized.store(true);
            std::cout << "Server started on " << address << ":" << port << std::endl;
            return true;
        } catch (std::exception &e) {
            std::cerr << "Error during server start: " << e.what() << std::endl;
            return false;
        }
    }

    void TcpServer::startAsyncAccept(ba::io_context *ioContext) {
        // Create shared pointer of new TcpConnection class instance
        auto newTcpConnection = std::make_shared<TcpConnection>(*ioContext);

        // Accept connection asynchronously via acceptor
        mpAcceptor->async_accept(newTcpConnection->getSocket(),
                                 [this, newTcpConnection, ioContext](const bs::error_code ec) {
                                     // Check for error codes and if TCP server is currently running
                                     if (!ec && mIsTcpServerRunning.load()) {
                                         this->handleAcceptedConnection(newTcpConnection, ioContext);
                                     } else if (ec) {
                                         std::cerr << "Accept error: " << ec.message() << std::endl;
                                     }
                                 });
    }


    void TcpServer::handleAcceptedConnection(const std::shared_ptr<TcpConnection> &connection,
                                             ba::io_context *ioContext) {
        // Try fetching unused connection id
        uint32_t connectionId;
        try {
            connectionId = getNextConnectionId();
        } catch (std::exception &e) {
            std::cerr << "TCP server accept connection error: " << e.what() << std::endl;
            // Retry after dealy
            std::this_thread::sleep_for(std::chrono::seconds(1));
            startAsyncAccept(ioContext);
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
            std::lock_guard<std::mutex> lock(mActiveConnectionsMutex);
            mActiveConnections[connectionId] = connection;
        }

        std::cout << "Connection " << connectionId
                << " accepted from: " << connection->getSocket().remote_endpoint() << std::endl;

        // Start reading incoming messages
        connection->read();
        // Start startAsyncAccept for further connections
        startAsyncAccept(ioContext);
    }


    uint32_t TcpServer::getNextConnectionId() {
        std::lock_guard<std::mutex> lock(mActiveConnectionsMutex);

        for (uint32_t id = 0; id < std::numeric_limits<uint32_t>::max(); ++id) {
            if (!mActiveConnections.contains(id)) {
                return id;
            }
        }

        throw std::runtime_error("Could not find free TCP connection id");
    }

    void TcpServer::removeActiveConnection(const uint32_t connectionId) {
        std::lock_guard<std::mutex> lock(mActiveConnectionsMutex);
        mActiveConnections.erase(connectionId);
    }

    void TcpServer::runTcpServer(ba::io_context *ioContext) {
        // Start accepting connections if TCP server is initialized
        if (mIsTcpServerInitialized.load()) {
            mIsTcpServerRunning.store(true);
            startAsyncAccept(ioContext);
        } else {
            std::cerr << "Tcp server run error: server not initialized" << std::endl;
        }
    }

    void TcpServer::stopTcpServer() {
        mIsTcpServerRunning.store(false);

        // Close acceptor
        if (mpAcceptor) {
            std::cout << "Stopping acceptor..." << std::endl;
            bs::error_code ec;
            mpAcceptor->cancel(ec);
            mpAcceptor->close(ec);
            mpAcceptor.reset();
            if (ec) {
                std::cerr << "Error during server close: " << ec.message() << std::endl;
            }
        }


        std::vector<std::shared_ptr<TcpConnection> > connectionsToClose;
        // Close all active connections
        {
            std::lock_guard<std::mutex> lock(mActiveConnectionsMutex);

            for (auto &weakConnection: mActiveConnections | std::views::values) {
                if (auto connection = weakConnection.lock()) {
                    connectionsToClose.push_back(connection);
                }
            }
            mActiveConnections.clear();
        }

        for (const auto &connection: connectionsToClose) {
            connection->close();
        }
    }

    bool TcpServer::isRunning() const {
        return mIsTcpServerRunning.load();
    }
}
