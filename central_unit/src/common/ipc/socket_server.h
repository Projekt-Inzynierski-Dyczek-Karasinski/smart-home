#pragma once

#include "socket_server_connection.h"

#include <utility>
#include <unordered_map>
#include <mutex>

#include <boost/asio.hpp>


namespace ba = boost::asio;
namespace bai = boost::asio::ip;

namespace SmartHome::IPC {
    /**
     * @brief Multi-protocol socket server for Smart Home IPC.
     *
     * @details Thread-safe singleton managing both TCP and Unix Domain Socket servers.
     *          Accepts incoming connections and manages their lifecycle.
     *          Supports multiple concurrent connections with unique IDs.
     *
     * @note Singleton pattern ensures single server instance.
     */
    class SocketServer {
    public:
        /**
         * @brief Server configuration structure.
         */
        struct Config {
            /**
             * @brief TCP server configuration.
             */
            struct Tcp {
                bool isEnabled = true; ///< Enable TCP acceptor
                std::string endpointAddress = "127.0.0.1"; ///< Server endpoint address
                int endpointPort = 43321; ///< Port number
            } tcp;

            /**
             * @brief Unix Domain Socket configuration.
             */
            struct Uds {
                bool isEnabled = true; ///< Enable UDS acceptor
                std::string endpointPath = "/var/run/smarthomed.sock"; ///< Socket file path
            } uds;
        };


        /**
         * @brief Get singleton instance.
         *
         * @details Thread-safe initialization using static local variable.
         *          Instance is created on first call and reused after.
         *
         * @return Reference to SocketServer singleton instance.
         */
        static SocketServer &Instance();

        // Delete copy constructor and assignment
        SocketServer(const SocketServer &) = delete;

        SocketServer &operator=(const SocketServer &) = delete;

        /**
         * @brief Initialize server with configuration.
         *
         * @param ioContext Boost::Asio IO context for async operations.
         * @param config Server configuration.
         *
         * @return true if at least one protocol initialized successfully.
         *
         * @post To start accepting connections, call runSocketServer().
         */
        bool initializeSocketServer(ba::io_context *ioContext, const Config &config);

        /**
         * @brief Start accepting connections.
         *
         * @details Begins asynchronous accept loops for enabled protocols.
         *          Must be called after successful initialization.
         *
         * @param ioContext Boost::Asio IO context for async operations.
         *
         * @pre initializeSocketServer() must be called successfully.
         */
        void runSocketServer(ba::io_context *ioContext);

        /**
         * @brief Stop server and close all connections.
         *
         * @details Stops accepting new connections and closes all active ones.
         *          Blocks until all connections are closed.
         */
        void stopSocketServer();

        /**
         * @brief Check if server is running.
         *
         * @return true if actively accepting connections.
         */
        bool isRunning() const;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        SocketServer();

        /**
         * @brief Private destructor for singleton pattern, stops TCP server.
         */
        ~SocketServer();

        /**
         * @brief Handle acceptor errors.
         */
        static void onAcceptError(const bs::error_code &ec);

        /**
         * @brief Common accept handler for both protocols.
         */
        void acceptorHandler(const std::shared_ptr<SocketServerConnection> &connection, ba::io_context *ioContext,
                             const bs::error_code &ec, SocketConnection::Type type);

        /**
         * @brief Start asynchronous TCP accept loop.
         *
         * @param ioContext Boost::Asio IO context for async operations.
         *
         */
        void startTcpAcceptor(ba::io_context *ioContext);


        /**
         * @brief Start asynchronous UDS accept loop.
         *
         * @param ioContext Boost::Asio IO context for async operations.
         *
         */
        void startUdsAcceptor(ba::io_context *ioContext);

        /**
         * @brief Handles newly accepted connection.
         *
         * @details Assigns connection ID, configures callback, saves connection, starts read loop.
         *          Schedules next accept operation to continue accepting new connections.
         *
         * @param connection Newly accepted connection.
         * @param ioContext Boost::Asio IO context for async operations.
         *
         * @note Called from async_accept completion handler (acceptorHandler).
         */
        void onAccept(const std::shared_ptr<SocketServerConnection> &connection, ba::io_context *ioContext);

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
         * @note Thread-safe via mutex.
         *       Called by TcpConnection close callback.
         */
        void removeActiveConnection(uint32_t connectionId);

        Config mConfig;

        /// Map of active connections (ID: weak_ptr)
        std::unordered_map<uint32_t, std::weak_ptr<SocketServerConnection> > mActiveConnections;
        std::mutex mActiveConnectionsMutex;

        // TCP
        std::unique_ptr<bai::tcp::acceptor> mpTcpAcceptor; ///< TCP acceptor for incoming connections.
        bai::tcp::endpoint mTcpEndpoint;

        //UDS
        std::unique_ptr<bal::stream_protocol::acceptor> mpUdsAcceptor; ///< UDS acceptor for incoming connections.
        bal::stream_protocol::endpoint mUdsEndpoint;

        // State flags
        std::atomic<bool> mIsSocketServerInitialized{false}; ///< Server initialized state.
        std::atomic<bool> mIsSocketServerRunning{false}; ///< Server running state.
    };
}
