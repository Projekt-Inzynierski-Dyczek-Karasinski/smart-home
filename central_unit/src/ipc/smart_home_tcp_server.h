#ifndef SMART_HOME_TCP_SERVER_H
#define SMART_HOME_TCP_SERVER_H
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace SmartHome::IPC {
    class TcpServer {
    public:
        static TcpServer &Instance();

        TcpServer(const TcpServer &) = delete;

        TcpServer &operator=(const TcpServer &) = delete;

        bool startTcpServer(ba::io_context *ioContext, unsigned short port = 43321);


        void runTcpServer(ba::io_context *ioContext);

        bool stopTcpServer();

        bool isRunning();

    private:
        TcpServer();

        ~TcpServer();

        void startAsyncAccept(ba::io_context *ioContext);

        std::unique_ptr<bip::tcp::acceptor> mpAcceptor;
        bip::tcp::endpoint mEndpoint;

        std::atomic<bool> mTcpServerInitialized{false};
        std::atomic<bool> mTcpServerRunning{false};
    };
}
#endif //SMART_HOME_TCP_SERVER_H
