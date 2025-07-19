#pragma once

#include "tcp_connection.h"

#include <atomic>
#include <memory>
#include <utility>
#include <unordered_map>
#include <mutex>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bip = boost::asio::ip;

namespace SmartHome::IPC {
    /**
     * @brief Asynchronous TCP server for inter-process communication.
     *
     * @details Thread-safe singleton managing asynchronous TCP server.
     *          Accepts incoming connections creating TcpConnection instances for each client.
     *          Supports muliple concurrent connections.
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
        void stopTcpServer();

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
         * @brief Start asynchronous accept loop.
         *
         * @details Creates new TcpConnection for each client. Registers async_accept handler.
         *          Accept loops ends on error or TCP server shutdown.
         *
         * @param ioContext IO context for async operations.
         *
         * @note Accept loop continues via handleAcceptedConnection() callback.
         */
        void startAsyncAccept(ba::io_context *ioContext);

        /**
         * @brief Handles newly accepted connection.
         *
         * @details Assigns connection ID, configures callback, saves connection, starts read loop.
         *          Schedules next accept operation to continue accepting new connections.
         *
         * @param connection Newly accepted connection.
         * @param ioContext IO context for scheduling next accept.
         *
         * @note Called from async_accept completion handler.
         */
        void handleAcceptedConnection(const std::shared_ptr<TcpConnection> &connection, ba::io_context *ioContext);

        /**
         * @brief Get next available connection ID.
         *
         * @details Finds first unsued ID in active connections map. Thread-safe via mutex.
         *
         *
         * @return Unique connection identifier.
         * @throw std::runtime_error if no free IDs available.
         */
        uint32_t getNextConnectionId();

        /**
         * @brief Remove connection from active connections map.
         *
         * @param connectionId ID of connections to remove.
         *
         * @note Called by TcpConnection close callback.
         */
        void removeActiveConnection(uint32_t connectionId);

        /// Map of active connections (ID: weak_ptr)
        std::unordered_map<uint32_t, std::weak_ptr<TcpConnection> > mActiveConnections;
        std::mutex mActiveConnectionsMutex;

        // std::vector<std::weak_ptr<TcpConnection>> mActiveConnections;
        // std::mutex mActiveConnectionsMutex;

        std::unique_ptr<bip::tcp::acceptor> mpAcceptor; ///< TCP acceptor for incoming connections.

        bip::tcp::endpoint mEndpoint; ///< Server endpoint.

        // State flags
        std::atomic<bool> mIsTcpServerInitialized{false}; ///< Server initialized state.
        std::atomic<bool> mIsTcpServerRunning{false}; ///< Server running state.
    };
}
