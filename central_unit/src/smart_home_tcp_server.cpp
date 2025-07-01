#include "smart_home_tcp_server.h"
#include  "smart_home_core.h"

#include <iostream>

namespace bs = boost::system;

namespace SmartHome {
    // ========================== Tcp Server ==========================
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
                                         std::cout << "Connection accepted from: " << newTcpConnection->getSocket().remote_endpoint() << std::endl;
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

    // ================================================================

    // ======================== Tcp Connection ========================
    TcpConnection::TcpConnection(ba::io_context &ioContext) : mSocket(ioContext) {
    };

    TcpConnection::~TcpConnection() {
        std::cout << "Connection destroyed." << std::endl;
        if (mSocket.is_open()) {
            bs::error_code ec;
            mSocket.close(ec);
            if (ec) {
                std::cerr << "Error during connection close: " << ec.message() << std::endl;
            }
        }
    }

    bip::tcp::socket &TcpConnection::getSocket() {
        return mSocket;
    }

    void TcpConnection::read() {
        auto self = shared_from_this();

        ba::async_read_until(mSocket, mStreamBuf, "\n", [this, self](bs::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                std::istream is(&mStreamBuf);
                std::string message;
                std::getline(is, message);

                std::cout << "Received: " << message << std::endl;
                read();
            } else {
                std::cerr << "Error during read: " << ec.message() << std::endl;
            }
        });
    }

    // ================================================================
}
