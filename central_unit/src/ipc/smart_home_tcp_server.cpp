#include "../smart_home_core.h"
#include "smart_home_tcp_server.h"
#include "smart_home_tcp_connection.h"

#include <iostream>

namespace bs = boost::system;

namespace SmartHome::IPC {
    TcpServer &TcpServer::Instance() {
        static TcpServer ServerInstance;
        return ServerInstance;
    }

    TcpServer::TcpServer() = default;

    TcpServer::~TcpServer() {
        if (mTcpServerInitialized.load() == true) {
            stopTcpServer();
        }
    }

    bool TcpServer::startTcpServer(ba::io_context *ioContext, unsigned short port) {
        try {
            mEndpoint = bip::tcp::endpoint(bip::tcp::v4(), port);
            mpAcceptor = std::make_unique<bip::tcp::acceptor>(*ioContext, mEndpoint);
            mTcpServerInitialized.store(true);
            std::cout << "Server started on port: " << port << std::endl;
            return true;
        } catch (std::exception &e) {
            std::cerr << "Error during server start: " << e.what() << std::endl;
            return false;
        }
    }

    void TcpServer::startAsyncAccept(ba::io_context *ioContext) {
        auto newTcpConnection = std::make_shared<TcpConnection>(*ioContext);

        mpAcceptor->async_accept(newTcpConnection->getSocket(),
                                 [this, newTcpConnection, ioContext](bs::error_code ec) {
                                     if (!ec && mTcpServerRunning.load() == true) {
                                         std::cout << "Connection accepted from: " << newTcpConnection->getSocket().
                                                 remote_endpoint() << std::endl;
                                         newTcpConnection->read();
                                         this->startAsyncAccept(ioContext);
                                     } else if (ec) {
                                         std::cerr << "Accept error: " << ec.message() << std::endl;
                                     }
                                 });
    }

    void TcpServer::runTcpServer(ba::io_context *ioContext) {
        if (mTcpServerInitialized.load() == true) {
            mTcpServerRunning.store(true);
            startAsyncAccept(ioContext);
        } else {
            std::cerr << "Tcp server run error: server not initialized" << std::endl;
        }
    }

    bool TcpServer::stopTcpServer() {
        mTcpServerRunning.store(false);

        if (mpAcceptor) {
            bs::error_code ec;
            mpAcceptor->close(ec);
            if (ec) {
                std::cerr << "Error during server close: " << ec.message() << std::endl;
            }
            mpAcceptor.reset();
            return true;
        }
        return false;
    }

    bool TcpServer::isRunning() {
        return mTcpServerRunning.load();
    }
}
