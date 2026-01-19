#include "uart_port.h"

namespace SmartHomeMediator {
    UartPort::UartPort(ba::io_context &ioContext, const std::string_view portName, const uint baudRate)
        : mPort(ioContext, portName.data()),
          mIoContext(ioContext),
          mBuffer(ms_BUFFER_CAPACITY) {
        // Configure UART with 8N1 format
        mPort.set_option(ba::serial_port::baud_rate(baudRate));
        mPort.set_option(ba::serial_port::character_size(8));
        mPort.set_option(ba::serial_port::parity(ba::serial_port::parity::none));
        mPort.set_option(ba::serial_port::stop_bits(ba::serial_port::stop_bits::one));
        mPort.set_option(ba::serial_port::flow_control(ba::serial_port::flow_control::none));

#ifdef __linux__
        const auto fd = mPort.native_handle();
        if (fd >= 0) {
            ::tcflush(fd, TCIOFLUSH);
        }
#endif
    }

    UartPort::~UartPort() {
        mIsShuttingDown = true;
        cancel();
        mPort.close();
    }

    ba::awaitable<void> UartPort::writeAsync(const std::vector<uint8_t> &data) {
        co_await ba::async_write(
            mPort,
            ba::buffer(data),
            ba::use_awaitable
        );
    }

    ba::awaitable<std::vector<uint8_t> > UartPort::readUntil(const std::chrono::milliseconds timeoutDuration) {
        mIsSyncModeActive = true; // Stop read loop

        cancel(); // Interrupt readLoop

        // Clear buffer
        mBuffer.consume(mBuffer.size());

        ba::steady_timer timer(mIoContext, timeoutDuration);
        bool timeout = false;

        timer.async_wait([this, &timeout](const bs::error_code &ec) {
            if (!ec) {
                timeout = true;
                cancel(); // Cancel the async read until operation
            }
        });

        std::vector<uint8_t> result;
        try {
            size_t bytesTransferred = co_await ba::async_read_until(
                mPort,
                mBuffer,
                ms_DELIMITER.data(),
                ba::use_awaitable
            );

            timer.cancel();

            //  Restart read loop
            mIsSyncModeActive = false;
            ba::post(mIoContext, [this] {
                if (!mIsShuttingDown) {
                    startReadLoop();
                }
            });

            const auto buffer = mBuffer.data();

            // Copy data without delimiter
            if (bytesTransferred >= ms_DELIMITER.size()) {
                result.assign(
                    ba::buffers_begin(buffer),
                    ba::buffers_begin(buffer) + (bytesTransferred - ms_DELIMITER.size())
                );
            }

            // Consume data including delimiter
            mBuffer.consume(bytesTransferred);

            co_return result;
        } catch (const boost::system::system_error &e) {
            timer.cancel();

            // Restart read loop
            mIsSyncModeActive = false;
            ba::post(mIoContext, [this] {
                if (!mIsShuttingDown) {
                    startReadLoop();
                }
            });

            // Return empty vector on timeout
            if (e.code() == ba::error::operation_aborted) {
                co_return result;
            }

            // Other errors propagate up
            throw;
        }
    }

    ba::awaitable<std::vector<uint8_t> > UartPort::readAsync() {
        ba::steady_timer timer(mIoContext);

        while (true) {
            const size_t currentSize = mBuffer.size();

            // Set timer for inactivity timeout
            auto timePerByte = static_cast<uint64_t>(
                static_cast<double>(ms_BITS_PER_BYTE) / getBaudRate() * ms_NANOSECONDS_PER_SECOND);
            auto timePerByteNs = std::chrono::nanoseconds(timePerByte);
            timer.expires_after(timePerByteNs * ms_TIME_PER_BYTE_MULTIPLIER + ms_READ_ASYNC_WAIT_MIN_TIMEOUT);
            // Wait for timeout
            try {
                co_await timer.async_wait(ba::use_awaitable);
            } catch (const bs::system_error &e) {
                // Timer cancelled - continue
                if (e.code() == ba::error::operation_aborted) {
                    continue;
                }
                throw; // Other errors propagate up
            }

            // Check if new data arrived
            size_t newSize = mBuffer.size();

            if (newSize == currentSize) {
                // No new data arrived for ms_READ_ASYNC_WAIT_TIMEOUT

                if (newSize == 0) {
                    // Continue until read any data
                    continue;
                }

                const auto buffer = mBuffer.data();
                std::vector<uint8_t> result(ba::buffers_begin(buffer), ba::buffers_end(buffer));

                mBuffer.consume(result.size());

                co_return result;
            }
        }
    }

    void UartPort::startReadLoop() {
        if (mIsShuttingDown || mIsSyncModeActive) {
            return;
        }

        mPort.async_read_some(
            mBuffer.prepare(ms_READ_CHUNK_SIZE),
            [this](const bs::error_code &ec, const size_t bytesTransferred) {
                readLoopCallback(ec, bytesTransferred);
            }
        );
    }

    void UartPort::setBaudRate(const uint baudRate) {
        cancel();

        mPort.set_option(ba::serial_port::baud_rate(baudRate));

        if (!mIsSyncModeActive && !mIsShuttingDown) {
            ba::post(mIoContext, [this] {
                startReadLoop();
            });
        }
    }

    uint UartPort::getBaudRate() const {
        ba::serial_port::baud_rate baudRate;
        mPort.get_option(baudRate);
        const int currentBaudRate = baudRate.value();
        return currentBaudRate;
    }

    void UartPort::cancel() {
        mPort.cancel();
    }

    void UartPort::readLoopCallback(const bs::error_code &ec, const size_t bytesTransferred) {
        if (ec) return;

        // Commit received data to buffer
        mBuffer.commit(bytesTransferred);

        // Chain next read
        if (!mIsSyncModeActive && !mIsShuttingDown) {
            startReadLoop();
        }
    }
}
