#ifndef SMART_HOME_CONNECTION_H
#define SMART_HOME_CONNECTION_H
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace SmartHome::IPC {
    class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
    public:
        TcpConnection(ba::io_context &ioContext);

        ~TcpConnection();

        bip::tcp::socket &getSocket();

        void read();

    private:
        bip::tcp::socket mSocket;
        ba::streambuf mStreamBuf;
    };
}
#endif //SMART_HOME_CONNECTION_H
