#include <cstddef>
#include <iosfwd>
#include <iostream>
#include <string>

#include "smart_home_tcp_connection.h"

namespace bs = boost::system;

namespace SmartHome::IPC {
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
}
