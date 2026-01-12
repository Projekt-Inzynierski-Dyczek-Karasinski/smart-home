#include "hc12_driver.h"

#include <algorithm>
#include <boost/algorithm/string/find.hpp>

namespace SmartHomeMediator {
    HC12Driver::HC12Driver(ba::io_context &ioContext,
                           const std::shared_ptr<su::Logger> &logger,
                           const std::string_view uartPortName,
                           const uint uartBaudRate)
        : mpLogger(logger),
          mIoContext(ioContext),
          mOriginalBaudRate(uartBaudRate),
          mLastWriteTime(std::chrono::steady_clock::now()) {
        try {
            mpUart = std::make_unique<UartPort>(mIoContext, uartPortName, uartBaudRate);

            constexpr bool initialPinStateHigh = true;
            constexpr bool setPinToOutput = true;
            mpSetPin = std::make_unique<GpioHandler>(ms_SET_PIN,
                                                     initialPinStateHigh,
                                                     setPinToOutput,
                                                     mUART_DEVICE_PATH.data());

            mpUart->startReadLoop();

            mpLogger->infof("[HC12_DRIVER] Driver initialized on %s at %u baud",
                            uartPortName.data(), uartBaudRate);
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] Failed to initialize: %s", e.what());
            throw;
        }
    }

    HC12Driver::~HC12Driver() {
        try {
            if (mIsInConfigMode) {
                mpSetPin->setHigh();
            }
        } catch (...) {
            // Ignore exceptions in destructor
        }
        mpLogger->info("[HC12_DRIVER] Driver shutdown");
    }

    ba::awaitable<void> HC12Driver::write(std::vector<uint8_t> data) {
        std::string tmp;
        for (const auto e : data) {
            tmp += std::to_string(e) + ",";
        }
        tmp.pop_back();
        mpLogger->debugf("[HC12_DRIVER] Write data: [%s]", tmp.data());

        // Calculate required delay based on FU mode
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastWriteTime);
        const auto required = getRequiredWriteDelay();

        // Wait if necessary
        if (elapsed < required) {
            try {
                const auto executor = co_await ba::this_coro::executor;
                ba::steady_timer timer(executor, required - elapsed);
                co_await timer.async_wait(ba::use_awaitable);
            } catch (const std::exception &e) {
                mpLogger->errorf("[HC12_DRIVER] Write delay error: %s", e.what());
                throw;
            }
        }

        // Write
        try {
            co_await mpUart->writeAsync(data);
            mLastWriteTime = std::chrono::steady_clock::now();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] Write error: %s", e.what());
            throw;
        }
    }

    ba::awaitable<std::vector<uint8_t> > HC12Driver::read() {
        try {
            co_return  co_await mpUart->readAsync();;
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] Read error: %s", e.what());
            throw;
        }
    }

    ba::awaitable<bool> HC12Driver::setOption(std::string option,
                                              std::string value) {
        // Convert to enum for validation
        const Hc12Option parsedOption = stringToOption(option);
        if (parsedOption == Hc12Option::UNDEFINED) {
            mpLogger->errorf("[HC12_DRIVER] Unknown option: %s", option.data());
            co_return false;
        }

        bool success = false;

        // Enter config mode
        try {
            co_await enterConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] setOption - enterConfig error: %s", e.what());
            co_return false;
        }

        // Build and send command
        try {
            const auto command = buildSetCommand(parsedOption, value);
            const auto response = co_await sendAtCommand(command, msCONFIG_TIMEOUT);

            const std::string responseStr = {response.begin(), response.end()};
            // TODO !pr add value comparison
            if (responseStr.starts_with("OK")) {
                success = true;
                mpLogger->debugf("[HC12_DRIVER] setOption success: %s=%s",
                                 option.data(), value.data());
            } else {
                mpLogger->warningf("[HC12_DRIVER] setOption failed: %s (response: %s)",
                                   option.data(), responseStr.c_str());
            }
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] setOption - sendCommand error: %s", e.what());
        }

        // Exit config mode
        try {
            co_await exitConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] setOption - exitConfig error: %s", e.what());
        }

        // Update cache on success
        if (success) {
            mOptionsCache[std::string(option)] = std::string(value);
        }

        co_return success;
    }

    ba::awaitable<bool> HC12Driver::setOption(const Hc12Option option, const std::string& value) {
        const auto optionStr = optionToString(option);
        return setOption(optionStr, value);
    }

    ba::awaitable<std::vector<uint8_t> > HC12Driver::getOption(const std::string option) {
        // Try cache first
        const auto iter = mOptionsCache.find(std::string(option));
        if (iter != mOptionsCache.end()) {
            mpLogger->debugf("[HC12_DRIVER] getOption cache found: %s=%s",
                             option.data(), iter->second.c_str());
            std::vector<uint8_t> result = {iter->second.begin(), iter->second.end()};
            co_return result;
        }

        // Convert to enum for validation
        const Hc12Option opt = stringToOption(option);
        if (opt == Hc12Option::UNDEFINED) {
            mpLogger->errorf("[HC12_DRIVER] getOption - unknown option: %s", option.data());
            co_return std::vector<uint8_t>();
        }

        std::vector<uint8_t> value;

        // Enter config mode - to send RY command
        try {
            co_await enterConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] getOption - enterConfig error: %s", e.what());
            co_return std::vector<uint8_t>();
        }

        // Query HC-12
        try {
            const auto command = buildGetCommand(opt);
            const auto response = co_await sendAtCommand(command, msCONFIG_TIMEOUT);
            value = parseResponse(response);

            if (!value.empty()) {
                mpLogger->debugf("[HC12_DRIVER] getOption cached: %s=%s",
                                 option.data(), value.data());
            }
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] getOption - sendCommand error: %s", e.what());
        }

        // Exit config mode
        try {
            co_await exitConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] getOption - exitConfig error: %s", e.what());
        }

        // Update cache on successful query
        if (!value.empty()) {
            mOptionsCache[std::string(option)] = {value.begin(), value.end()};
        }

        co_return value;
    }

    ba::awaitable<std::string> HC12Driver::getAllOptions() {
        std::string formatted;

        //TODO !pr implement

        // const auto options = {Hc12Option::CHANNEL, Hc12Option::BAUDRATE, Hc12Option::FU_MODE, Hc12Option::POWER};
        //
        // for (auto option: options) {
        // }
        //
        // getOption();

        co_return formatted;
    }

    ba::awaitable<void> HC12Driver::enterConfigMode() {
        if (mIsInConfigMode) {
            co_return;
        }

        // Set pin LOW to enter config mode
        mpSetPin->setLow();

        // Wait 40ms as per HC-12 specification
        ba::steady_timer timer(mIoContext, 40ms);
        co_await timer.async_wait(ba::use_awaitable);

        mIsInConfigMode = true;
        mpLogger->debugf("[HC12_DRIVER] Entered config mode");
    }

    ba::awaitable<void> HC12Driver::exitConfigMode() {
        if (!mIsInConfigMode) {
            co_return;
        }

        // Set pin HIGH to exit config mode
        mpSetPin->setHigh();

        // Wait 80ms as per HC-12 specification
        ba::steady_timer timer(mIoContext, 80ms);
        co_await timer.async_wait(ba::use_awaitable);

        // Restore original baud rate
        mpUart->setBaudRate(mOriginalBaudRate);

        mIsInConfigMode = false;
        mpLogger->debugf("[HC12_DRIVER] Exited config mode");
    }

    ba::awaitable<std::vector<uint8_t> > HC12Driver::sendAtCommand(std::vector<uint8_t> command,
                                                                   const std::chrono::milliseconds timeout) const {
        co_await mpUart->writeAsync(command);

        auto response = co_await mpUart->readUntil(timeout);

        mpLogger->debugf("[HC12_DRIVER] AT command: %s -> %s", command.data(), response.data());

        co_return response;
    }

    auto HC12Driver::stringToOption(const std::string_view option) -> HC12Driver::Hc12Option {
        if (option == "channel") return Hc12Option::CHANNEL;
        if (option == "baudrate") return Hc12Option::BAUDRATE;
        if (option == "fu_mode") return Hc12Option::FU_MODE;
        if (option == "power") return Hc12Option::POWER;
        return Hc12Option::UNDEFINED;
    }

    std::string HC12Driver::optionToString(const Hc12Option option) {
        switch (option) {
            case Hc12Option::CHANNEL: return "channel";
            case Hc12Option::BAUDRATE: return "baudrate";
            case Hc12Option::FU_MODE: return "fu_mode";
            case Hc12Option::POWER: return "power";
            default: return "undefined";
        }
    }

    std::vector<uint8_t> HC12Driver::buildSetCommand(const Hc12Option option,
                                                     const std::string_view value) {
        std::string commandStr;

        switch (option) {
            case Hc12Option::CHANNEL:
                commandStr = "AT+C" + std::string(3 - value.length(), '0') + std::string(value);
                break;
            case Hc12Option::BAUDRATE:
                commandStr = "AT+B" + std::string(value);
                break;
            case Hc12Option::FU_MODE:
                commandStr = "AT+FU" + std::string(value);
                break;
            case Hc12Option::POWER:
                commandStr = "AT+P" + std::string(value);
                break;
            default:
                commandStr = "";
        }

        return {commandStr.begin(), commandStr.end()};
    }

    std::vector<uint8_t> HC12Driver::buildGetCommand(const Hc12Option option) {
        std::string commandStr;

        switch (option) {
            case Hc12Option::CHANNEL:
                commandStr = "AT+RC";
                break;
            case Hc12Option::BAUDRATE:
                commandStr = "AT+RB";
                break;
            case Hc12Option::FU_MODE:
                commandStr = "AT+RF";
                break;
            case Hc12Option::POWER:
                commandStr = "AT+RP";
                break;
            default:
                commandStr = "AT";
        }

        return {commandStr.begin(), commandStr.end()};
    }

    std::vector<uint8_t> HC12Driver::parseResponse(const std::vector<uint8_t> &response) {
        // Expected formats:
        // "OK+RC042" -> "42"
        // "OK+B9600" -> "9600"
        // "OK+FU3" -> "3"
        // "OK+RP:+20 dBm" -> "8"

        const std::string responseStr = {response.begin(), response.end()};

        if (!responseStr.starts_with("OK")) {
            return {};
        }

        // Special case for power response
        if (responseStr.find("RP:") != std::string_view::npos) {
            // Extract dBm value and convert to level
            auto dbmPos = responseStr.find_last_of("+-");

            if (dbmPos != std::string_view::npos) {
                dbmPos = dbmPos + 1; // Shift position to first number of dBm
                const auto dbmEnd = responseStr.find(' ', dbmPos);
                const std::string dbmStr(responseStr.substr(dbmPos, dbmEnd - dbmPos));
                return dbmToPowerLevel(dbmStr);
            }
            return {};
        }

        // Standard response: extract value after "OK+"
        const auto plusPos = responseStr.find('+');
        if (plusPos == std::string_view::npos) {
            return {};
        }


        // Find where the value starts
        std::string valuePart(responseStr.substr(plusPos + 1));

        // Remove command prefix
        if (valuePart.starts_with("RC") || valuePart.starts_with("RF") ||
            valuePart.starts_with("RP") || valuePart.starts_with("FU")) {
            valuePart = valuePart.substr(2);
        } else if (valuePart.starts_with("C") || valuePart.starts_with("B") ||
                   valuePart.starts_with("P")) {
            valuePart = valuePart.substr(1);
        }

        // Remove leading zeros
        const auto firstNonZero = valuePart.find_first_not_of('0');
        if (firstNonZero != std::string::npos) {
            valuePart = valuePart.substr(firstNonZero);
        }

        return {valuePart.begin(), valuePart.end()};
    }

    std::chrono::milliseconds HC12Driver::getRequiredWriteDelay() const {
        // Check FU mode from cache
        const auto iter = mOptionsCache.find("fu_mode");
        if (iter != mOptionsCache.end()) {
            const std::string &mode = iter->second;
            if (mode == "2" || mode == "4") {
                return 2000ms; // Value from HC12 user manual
            }
        }

        // Default for FU1, FU3, or unknown
        return 80ms; // Most reliable value (lower values caused data loss)
    }

    std::string_view HC12Driver::powerLevelToDbm(const std::string_view level) {
        int parsedLevel = -1;

        try {
            parsedLevel = std::stoi(level.data());
        } catch (...) {
        }

        if (parsedLevel >= 0 && parsedLevel < mDbmArray.size()) {
            return mDbmArray[parsedLevel];
        }
        return mDbmArray[mDbmArray.size() - 1];
    }

    std::vector<uint8_t> HC12Driver::dbmToPowerLevel(const std::string_view dbm) {
        const auto iter = std::ranges::find(mDbmArray, dbm);
        if (iter != mDbmArray.end()) {
            std::string_view powerLevelView = iter->data();
            return {powerLevelView.begin(), powerLevelView.end()};
        }

        return {};
    }
}
