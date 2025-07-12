#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace SmartHome::IPC {
    /**
     * @brief TCP server for IPC.
     *
     * @details Thread-safe singleton managing asynchronous TCP server.
     *          Accepts incoming connections creating TcpConnection instances for each client.
     */
    class TcpServer {
    public:
        /**
         * @brief Get singleton instance of TcpServer.
         *
         * @details Thread-safe initialization using static local variable.
         *          Instance is created on first call and reused after.
         *
         * @return Reference to TcpServer singleton instance.
         */
        static TcpServer &Instance();

        // Prevent copying
        TcpServer(const TcpServer &) = delete;

        // Prevent assignment
        TcpServer &operator=(const TcpServer &) = delete;

        /**
         * @brief Initialize TCP server with endpoint configuration.
         *
         * @param ioContext boost.asio IO context for async operations.
         * @param address Endpoint IP address.
         * @param port Endpoint port number.
         *
         * @return true if successful, false on error.
         */
        bool startTcpServer(ba::io_context *ioContext, const std::string &address, const unsigned short &port);

        /**
         * @brief Start accepting connections.
         *
         * @details Begins asynchronous connection accept loop.
         *          Each accepted connection is handled asynchronously by a TcpConnection instance.
         *
         * @param ioContext boost.asio IO context for async operations.
         *
         * @pre startTcpServer() must be called successfully.
         */
        void runTcpServer(ba::io_context *ioContext);

        /**
         * @brief Stops accepting new connections and close acceptor.
         *
         * @return true if stopped successfully, false on error or not running.
          */
        bool stopTcpServer();

        /**
         * @brief Check if server is currently running.
         *
         * @return true if server is running, false otherwise.
         */
        bool isRunning() const;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        TcpServer();

        /**
         * @brief Private destructor for singleton pattern, stops TCP server.
         */
        ~TcpServer();

        /**
         * @brief Internal async accept loop.
         *
         * @details Creates new TcpConnection for each client.
         *          Chains next accept operation to handle further connection.
         *          Stops on TCP server shutdown or on error.
         *
         * @param ioContext boost.asio IO context for async operations.
         */
        void startAsyncAccept(ba::io_context *ioContext);

        std::unique_ptr<bip::tcp::acceptor> mpAcceptor; ///< TCP acceptor for incoming connections.

        bip::tcp::endpoint mEndpoint; ///< Server endpoint.

        // State flags
        std::atomic<bool> mTcpServerInitialized{false}; ///< Server initialized state.
        std::atomic<bool> mTcpServerRunning{false}; ///< Server running state.
    };
}
