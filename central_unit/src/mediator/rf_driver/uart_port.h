#pragma once

#include <vector>
#include <utility>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bs = boost::system;

namespace SmartHomeMediator {
    using namespace std::chrono_literals;

    /**
     * @brief UART wrapper with coroutine-based operations.
     *
     * @details Background read loop continuously fills internal buffer.
     *          Methods use coroutines for sequential-looking async code.
     *
     * @note Must be run on single-threaded io_context.
     */
    class UartPort {
    public:
        /**
         * @brief Construct and open UART port.
         *
         * @param ioContext Single-threaded IO context for all operations.
         * @param portName Device path (e.g. "/dev/serial0").
         * @param baudRate Initial baud rate.
         */
        UartPort(ba::io_context &ioContext, std::string_view portName, uint baudRate);

        ~UartPort();

        UartPort(const UartPort &) = delete;

        UartPort &operator=(const UartPort &) = delete;

        /**
         * @brief Write data asynchronously.
         *
         * @param data Data to write.
         *
         * @return Awaitable that completes when write finishes or on error.
         *
         * @throws bs::system_error when awaitable operations throws error.
         */
        ba::awaitable<void> writeAsync(const std::vector<uint8_t> &data);

        /**
         * @brief Read until delimiter with timeout (for pseudo synchronous read).
         *
         * @details Stops read loop, clears buffer, reads until CRLF.
         *          Uses timer for timeout. Restarts read loop after completion.
         *
         * @param timeout Max wait time for response.
         *
         * @return Data without delimiter, or empty string on timeout/error.
         *
         * @throws bs::system_error when awaitable operations throws error.
         */
        ba::awaitable<std::vector<uint8_t>> readUntilAsync(std::chrono::milliseconds timeout);

        /**
         * @brief Read with grouping timeout.
         *
         * @details Monitors buffer filled by readLoop. When no new data arrives for \c ms_READ_ASYNC_WAIT_TIMEOUT ms,
         *          returns all accumulated data.
         *
         * @return Accumulated data from buffer.
         *
         * @throws bs::system_error when awaitable operations throws error.
         */
        ba::awaitable<std::vector<uint8_t>> readAsync();

        /**
         * @brief Start background read loop.
         *
         * @details Continuously reads into buffer. Automatically chains unless mSyncModeActive or shutdown.
         */
        void startReadLoop();

        /**
         * @brief Change baud rate.
         *
         * @param baudRate New baud rate.
         */
        void setBaudRate(uint baudRate);

        /**
         * @brief Cancel all pending operations on port.
         */
        void cancel();

    private:
        /**
         * @brief Background loop callback.
         *
         * @details Continuously reads into mBuffer. Chains itself unless mSyncModeActive or shutdown.
         *
         */
        void readLoopCallback(const bs::error_code &ec, size_t bytes_transferred);

        static constexpr std::string_view ms_DELIMITER = "\r\n";

        static constexpr size_t ms_BUFFER_CAPACITY = 1024;
        static constexpr size_t ms_READ_CHUNK_SIZE = 512;

        static constexpr auto ms_READ_ASYNC_WAIT_TIMEOUT = 100ns;

        ba::serial_port mPort;
        ba::io_context &mIoContext;

        /// Buffer continuously filled by readLoop
        ba::streambuf mBuffer;

        /// Flag to pause readLoop during sync operations
        bool mSyncModeActive = false;

        /// For shutdown handling
        bool mIsShuttingDown = false;
    };
}
