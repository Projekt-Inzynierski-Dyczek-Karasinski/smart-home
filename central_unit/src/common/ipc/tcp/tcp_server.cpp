#include "tcp_server.h"
#include "tcp_connection.h"

#include <iostream>

namespace bs = boost::system;

namespace SmartHome::IPC {
    TcpServer &TcpServer::Instance() {
        static TcpServer ServerInstance;
        return ServerInstance;
    }

    TcpServer::TcpServer() = default;

    TcpServer::~TcpServer() {
        if (mIsTcpServerInitialized.load()) {
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
                                         std::cout << "Connection accepted from: " << newTcpConnection->getSocket().
                                                 remote_endpoint() << std::endl;
                                         // Start reading incoming messages
                                         newTcpConnection->read();
                                         // Start startAsyncAccept for further connections
                                         this->startAsyncAccept(ioContext);
                                     } else if (ec) {
                                         std::cerr << "Accept error: " << ec.message() << std::endl;
                                     }
                                 });
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

    bool TcpServer::stopTcpServer() {
        mIsTcpServerRunning.store(false);

        // Close acceptor
        if (mpAcceptor != nullptr) {
            bs::error_code ec;
            mpAcceptor->close(ec);
            mpAcceptor.reset();
            if (ec) {
                std::cerr << "Error during server close: " << ec.message() << std::endl;
                return false;
            }
            return true;
        }
        return false;
    }

    bool TcpServer::isRunning() const {
        return mIsTcpServerRunning.load();
    }
}
