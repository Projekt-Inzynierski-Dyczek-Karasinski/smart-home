#include "tcp_connection.h"

#include <cstddef>
#include <iosfwd>
#include <iostream>
#include <string>


namespace bs = boost::system;

namespace SmartHome::IPC {
    TcpConnection::TcpConnection(ba::io_context &ioContext) : mSocket(ioContext) {
    }

    TcpConnection::~TcpConnection() {
        std::cout << "Connection destroyed." << std::endl;
        if (mSocket.is_open()) {
            bs::error_code ec;
            mSocket.shutdown(bip::tcp::socket::shutdown_both, ec);
            mSocket.close(ec);
            if (ec) {
                std::cerr << "TcpConnection destruction error: " << ec.message() << std::endl;
            }
        }
    }

    bip::tcp::socket &TcpConnection::getSocket() {
        return mSocket;
    }

    void TcpConnection::read() {
        if (mIsShuttingDown.load()) return; //Prevents starting new read during shutdown

        // Define shared pointer for this connection
        auto self = shared_from_this(); //TODO consider making this private class variable

        //Start asynchronously reading incoming data
        ba::async_read_until(mSocket, mStreamBuf, "\n", [this, self](const bs::error_code ec, std::size_t) {
            if (!ec && !mIsShuttingDown.load()) {
                std::istream is(&mStreamBuf);
                std::string message;
                std::getline(is, message);

                std::cout << "Received: " << message << std::endl;
                read();
            } else if (ec == ba::error::operation_aborted) {
                std::cout << "TcpConnection read operation canceled" << std::endl;
            } else {
                //TODO add different handling for end of file error
                std::cerr << "TcpConnection read error: " << ec.message() << std::endl;
            }
        });
    }

    void TcpConnection::close() {
        bool expected = false;
        if (!mIsShuttingDown.compare_exchange_strong(expected, true)) {
            return; // Is already shutting down
        }

        if (mSocket.is_open()) {
            bs::error_code ec;
            mSocket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            mSocket.close(ec);
            if (ec) {
                std::cerr << "TcpConnection shutdown error: " << ec.message() << std::endl;
            }
        }

        if (mCloseCallback) {
            const auto callback = std::move(mCloseCallback);
            callback(mConnectionId);
        }
    }

    void TcpConnection::setId(const uint32_t &connectionId) {
        mConnectionId = connectionId;
    }

    void TcpConnection::setCloseCallback(std::function<void(uint32_t)> callback) {
        mCloseCallback = std::move(callback);
    }
}
