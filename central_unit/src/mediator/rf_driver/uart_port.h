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
         * @details Stops read loop, clears buffer, reads until \c msDELIMITER.
         *          Uses timer for timeout. Restarts read loop after completion.
         *
         * @param timeoutDuration Max wait time for response.
         *
         * @return Data without delimiter, or empty string on timeout/error.
         *
         * @throws bs::system_error when awaitable operations throws error.
         */
        ba::awaitable<std::vector<uint8_t> > readUntil(std::chrono::milliseconds timeoutDuration);

        /**
         * @brief Read with grouping timeout.
         *
         * @details Monitors buffer filled by readLoop. When no new data arrives for
         *          \code
         *              (msBITS_PER_BYTE / [baud rate] * msNANOSECONDS_PER_SECOND)ns
         *              * msTIME_PER_BYTE_MULTIPLIER + msREAD_ASYNC_WAIT_MIN_TIMEOUT
         *          \endcode
         *          returns all accumulated data.
         *
         * @return Accumulated data from buffer.
         *
         * @throws bs::system_error when awaitable operations throws error.
         */
        ba::awaitable<std::vector<uint8_t> > readAsync();

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
         *
         * @throws bs::system_error when baud rate change fails.
         */
        void setBaudRate(uint baudRate);

        /**
         * @brief Current UART baud rate getter.
         *
         * @return Current baud rate.
         */
        [[nodiscard]] uint getBaudRate() const;

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
        void readLoopCallback(const bs::error_code &ec, size_t bytesTransferred);

        static constexpr std::string_view msDELIMITER = "\r\n";

        static constexpr size_t msBUFFER_CAPACITY = 1024;
        static constexpr size_t msREAD_CHUNK_SIZE = 512;

        static constexpr auto msNANOSECONDS_PER_SECOND = 1'000'000'000;
        static constexpr auto msREAD_ASYNC_WAIT_MIN_TIMEOUT = 5ms; // TODO test with lower values
        static constexpr auto msTIME_PER_BYTE_MULTIPLIER = 10; // TODO test with lower values
        static constexpr auto msBITS_PER_BYTE = 10; // 8N1

        ba::serial_port mPort;
        ba::io_context &mIoContext;

        /// Buffer continuously filled by readLoop
        ba::streambuf mBuffer;

        /// Flag to pause readLoop during sync operations
        std::atomic_bool mIsSyncModeActive = false;

        /// For shutdown handling
        std::atomic_bool mIsShuttingDown = false;
    };
}
