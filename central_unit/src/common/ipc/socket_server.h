#pragma once

#include "socket_server_connection.h"
#include "async_logger.h"
#include "api.h"

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
         * @param api Reference to an API class implementation instance.
         * @param logger Logger shared pointer instance pointer.
         *
         * @return true if at least one protocol initialized successfully.
         *
         * @post To start accepting connections, call runSocketServer().
         */
        bool initializeSocketServer(ba::io_context *ioContext,
                                    const Config &config,
                                    const std::shared_ptr<API::Api> &api,
                                    const std::shared_ptr<Utils::Logger> &logger);

        /**
         * @brief Start accepting connections.
         *
         * @details Begins asynchronous accept loops for enabled protocols.
         *          Must be called after successful initialization.
         *
         *
         * @pre initializeSocketServer() must be called successfully.
         */
        void runSocketServer();

        /**
         * @brief Stop server and close all connections.
         *
         * @details Stops accepting new connections and closes all active ones.
         *          Blocks until all connections are closed.
         */
        void stopSocketServer();

        /**
         * @brief Stops acceptors and signals end of accepting new connections.
         */
        void stopAcceptors();

        //TODO Sockets do not close for incoming traffic consistently - more testing needed
        /**
         * @brief Closes connections sockets for incoming traffic.
         */
        void stopIncomingTraffic();

        /**
         * @brief Check if server is running.
         *
         * @return true if actively accepting connections.
         */
        bool isRunning() const;

        /**
         * @brief Connection getter.
         *
         * @param connectionId Connection to get.
         * @return Shared pointer to SocketServerConnection instance.
         */
        std::shared_ptr<SocketServerConnection> getConnection(connectionId_t connectionId);

        /**
         * @brief IO context getter.
         *
         * @return SocketServer IO context pointer.
         */
        ba::io_context *getIoContext() const;

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
         *
         * @param ec Boost::System error code.
         */
        void onAcceptError(const bs::error_code &ec) const;

        /**
         * @brief Common accept handler for both protocols.
         *
         * @param connection Connection instance shared pointer reference.
         * @param ec Boost::System error code.
         * @param type Acceptor type.
         */
        void acceptorHandler(const std::shared_ptr<SocketServerConnection> &connection,
                             const bs::error_code &ec,
                             SocketConnection::Type type);

        /**
         * @brief Start asynchronous TCP accept loop.
         *
         */
        void startTcpAcceptor();


        /**
         * @brief Start asynchronous UDS accept loop.
         *
         */
        void startUdsAcceptor();

        /**
         * @brief Handles newly accepted connection.
         *
         * @details Assigns connection ID, configures callback, saves connection, starts read loop.
         *          Schedules next accept operation to continue accepting new connections.
         *
         * @param connection Newly accepted connection.
         *
         * @note Called from async_accept completion handler (acceptorHandler).
         */
        void onAccept(const std::shared_ptr<SocketServerConnection> &connection);

        /**
         * @brief Get next available connection ID.
         *
         * @details Finds first unsued ID in active connections map. Thread-safe via mutex.
         *
         *
         * @return Unique connection identifier.
         * @throw std::runtime_error if no free IDs available.
         */
        connectionId_t getNextConnectionId();

        /**
         * @brief Remove connection from active connections map.
         *
         * @param connectionId ID of connections to remove.
         *
         * @note Thread-safe via mutex.
         *       Called by TcpConnection close callback.
         */
        void removeActiveConnection(connectionId_t connectionId);

        ba::io_context *mpIoContext;
        Config mConfig;
        std::shared_ptr<Utils::Logger> mpLogger; ///< Logger instance shared pointer
        std::shared_ptr<API::Api> mpApi;

        /// Map of active connections (ID: weak_ptr)
        std::unordered_map<connectionId_t, std::weak_ptr<SocketServerConnection> > mActiveConnections;
        std::mutex mActiveConnectionsMutex;

        // TCP
        std::unique_ptr<bai::tcp::acceptor> mpTcpAcceptor; ///< TCP acceptor for incoming connections.
        bai::tcp::endpoint mTcpEndpoint;

        // UDS
        std::unique_ptr<bal::stream_protocol::acceptor> mpUdsAcceptor; ///< UDS acceptor for incoming connections.
        bal::stream_protocol::endpoint mUdsEndpoint;

        // State flags
        std::atomic<bool> mIsSocketServerInitialized{false}; ///< Server initialized state.
        std::atomic<bool> mIsSocketServerRunning{false}; ///< Server running state.
        std::atomic<bool> mIsAcceptingNewConnections{false};
    };
}
