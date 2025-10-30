#include "rf_client.h"

#include <chrono>

namespace SmartHomeMediator {
    RfClient::RfClient(ba::io_context &ioContext,
                       const std::shared_ptr<su::Logger> &logger,
                       const std::string_view uartPortName,
                       const uint uartBaudRate)
        : mIoContext(ioContext), mpLogger(logger) {
        try {
            mpDriver = std::make_unique<HC12Driver>(
                mIoContext,
                mpLogger,
                uartPortName,
                uartBaudRate
            );

            mpLogger->info("[RF_CLIENT] RF client created");
        } catch (const std::exception &e) {
            mpLogger->errorf("[RF_CLIENT] Failed to create RF client: %s", e.what());
            throw;
        }
    }

    RfClient::~RfClient() {
        mIsReceiving = false;
        mpDriver.reset();
        mpLogger->info("[RF_CLIENT] RF client destroyed");
    }

    void RfClient::initialize(const std::function<void(bool)> &onComplete) const {
        mpLogger->info("[RF_CLIENT] Initializing - setting channel to default");

        ba::co_spawn(mIoContext, [this, onComplete]() -> ba::awaitable<void> {
            bool success = false;

            // Set channel to default
            try {
                success = co_await mpDriver->setOption("channel", "1");
            } catch (const std::exception &e) {
                mpLogger->errorf("[RF_CLIENT] Initialization error: %s", e.what());
                onComplete(false);
                co_return;
            }

            if (success) {
                mpLogger->info("[RF_CLIENT] Initialization complete - channel set to default");
            } else {
                mpLogger->error("[RF_CLIENT] Initialization failed - could not set channel");
            }

            onComplete(success);
            co_return;
        }, ba::detached);
    }

    void RfClient::startReceiving(const std::function<void(const std::vector<uint8_t> &)> &onDataReceived) {
        if (mIsReceiving) {
            mpLogger->warning("[RF_CLIENT] Already receiving");
            return;
        }

        mIsReceiving = true;
        mpLogger->info("[RF_CLIENT] Starting receive loop (echo mode)");

        ba::co_spawn(mIoContext, receiveLoop(onDataReceived), ba::detached);
    }

    void RfClient::send(const std::vector<uint8_t> &data,
                        const std::function<void(bool)> &onComplete) const {
        ba::co_spawn(mIoContext, [this, data, onComplete]() -> ba::awaitable<void> {
            bool success = false;

            // Write data
            try {
                co_await mpDriver->write(data);
                success = true;
                mpLogger->debugf("[RF_CLIENT] Sent %zu bytes", data.size());
            } catch (const std::exception &e) {
                mpLogger->errorf("[RF_CLIENT] Send error: %s", e.what());
            }

            onComplete(success);
            co_return;
        }, ba::detached);
    }

    ba::awaitable<void> RfClient::receiveLoop(std::function<void(const std::vector<uint8_t> &)> onDataReceived) const {
        int tmpMessageCount = 0;
        while (mIsReceiving) {
            std::vector<uint8_t> data;
            bool read_error = false;

            try {
                data = co_await mpDriver->read();
            } catch (const std::exception &e) {
                mpLogger->errorf("[RF_CLIENT] Receive error: %s", e.what());
                read_error = true;
            }

            if (read_error) {
                ba::steady_timer timer(mIoContext, std::chrono::milliseconds(100));
                try {
                    co_await timer.async_wait(ba::use_awaitable);
                } catch (...) {
                    break;
                }
                continue;
            }

            if (data.empty()) {
                continue;
            }

            std::string tmpStr;
            for (auto byte: data) {
                tmpStr += std::to_string(byte);
            }

            mpLogger->debugf("[RF_CLIENT] [MESSAGE_%d] Received %zu bytes: %s",
                             tmpMessageCount++,
                             data.size(), tmpStr.c_str());

            onDataReceived(data);

            // try {
            //     std::vector<uint8_t> echo_data(data.begin(), data.end());
            //     co_await mpDriver->write(echo_data);
            //
            //     mpLogger->debugf("[RF_CLIENT] Echoed back %zu bytes", echo_data.size());
            // } catch (const std::exception &e) {
            //     mpLogger->errorf("[RF_CLIENT] Echo error: %s", e.what());
            // }
        }

        mpLogger->info("[RF_CLIENT] Receive loop stopped");
        co_return;
    }
}
