#include "hc12_driver.h"
#include "../rf_api/rf_types.h"

#include <algorithm>
#include <boost/algorithm/string/find.hpp>


namespace SmartHomeMediator {
    HC12Driver::HC12Driver(ba::io_context &ioContext,
                           const std::shared_ptr<su::Logger> &logger,
                           Config config)
        : mpLogger(logger),
          mIoContext(ioContext),
          mOriginalBaudRate(config.uartBaudrate),
          mLastWriteTime(std::chrono::steady_clock::now()) {
        // Try initializing
        try {
            constexpr bool setPinToOutput = true;
            constexpr bool initialPinStateHigh = true;

            mpUart = std::make_unique<UartPort>(mIoContext, config.uartPortPath, config.uartBaudrate);

            mpSetPin = std::make_unique<GpioHandler>(config.setPin,
                                                     initialPinStateHigh,
                                                     setPinToOutput,
                                                     config.chipPath);
            mpUart->startReadLoop();

            mpLogger->infof("[HC12_DRIVER] Driver initialized on %s at %u baud",
                            config.uartPortPath.c_str(),
                            config.uartBaudrate);
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] Failed to initialize: %s", e.what());
            throw;
        }
    }

    HC12Driver::~HC12Driver() {
        try {
            if (mIsInConfigMode.load(std::memory_order::acquire)) {
                mpSetPin->setHigh();
            }
        } catch (...) {
            // Ignore exceptions in destructor
        }
        mpLogger->debug("[HC12_DRIVER] Driver shutdown");
    }

    ba::awaitable<void> HC12Driver::write(const std::vector<uint8_t> data) {
        // TODO left for debug, remove before merging with main
        if (mpLogger->getLevel() == SmartHome::Utils::LogLevels::Level::DEBUG) {
            std::string tmp;
            for (const auto e: data) {
                tmp += std::to_string(e) + ",";
            }
            tmp.pop_back();
            mpLogger->debugf("[HC12_DRIVER] Write data: [%s]", tmp.data());
        }

        // Calculate required delay
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastWriteTime);
        const auto required = getRequiredWriteDelay();

        // Wait if necessary
        if (elapsed < required) {
            const auto executor = co_await ba::this_coro::executor;
            ba::steady_timer timer(executor, required - elapsed);
            co_await timer.async_wait(ba::use_awaitable);
        }

        co_await mpUart->writeAsync(data);
        mLastWriteTime = std::chrono::steady_clock::now();
    }

    ba::awaitable<std::vector<uint8_t> > HC12Driver::read() {
        co_return co_await mpUart->readAsync();;
    }

    ba::awaitable<bool> HC12Driver::setOption(std::string option,
                                              std::string value) {
        // Convert to enum for validation
        const Hc12Option parsedOption = stringToOption(option);
        if (parsedOption == Hc12Option::UNDEFINED) {
            throw std::invalid_argument("setOption error: unknown option");
        }

        char buffer[1024];
        int valueNum = -1;

        if (parsedOption != Hc12Option::DEFAULT) {
            try {
                valueNum = std::stoi(value);
            } catch (const std::exception &e) {
                sprintf(buffer, "setOption received invalid value: expected numeric: %s", e.what());
                throw std::invalid_argument(buffer);
            }
        } else {
            // Clear cache on reset to default
            mOptionsCache.clear();
        }

        // Validate option value
        switch (parsedOption) {
            case Hc12Option::BAUDRATE:
                if (!msVALID_BAUDRATE.contains(valueNum))
                    throw std::invalid_argument("BAUDRATE received invalid value");
                break;
            case Hc12Option::CHANNEL:
                if (!isChannelValid(valueNum)) throw std::invalid_argument("CHANNEL received invalid value");
                break;
            case Hc12Option::FU_MODE:
                if (!msVALID_FU_MODE.contains(valueNum)) throw std::invalid_argument("FU_MODE received invalid value");
                break;
            case Hc12Option::POWER:
                if (!ms_VALID_POWER.contains(valueNum)) throw std::invalid_argument("POWER received invalid value");
                break;
            default:
                break;
        }

        bool success = false;

        // Enter config mode
        try {
            co_await enterConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] [SET_OPTION] enterConfig error: %s", e.what());
            throw;
        }

        int currentBaudRate = -1; // Default to negative number (invalid baud rate)
        if (parsedOption == Hc12Option::BAUDRATE) {
            currentBaudRate = mpUart->getBaudRate();
            mpLogger->debugf("[HC12_DRIVER] [SET_OPTION] current baudrate %d", currentBaudRate);
        }

        // Build and send command
        try {
            auto command = buildSetCommand(parsedOption, value);
            auto response = co_await sendAtCommand(command, msCONFIG_TIMEOUT);

            // Special case for setting and validating baud rate
            if (parsedOption == Hc12Option::BAUDRATE) {
                // Exit and reenter config mode before validating baud rate change
                try {
                    co_await exitConfigMode();
                } catch (const std::exception &e) {
                    throw;
                }

                // Change baudrate to send end receive in new baud rate
                mpUart->setBaudRate(valueNum);

                // Delay for baud rate change, and config mode reenter
                ba::steady_timer timer(co_await ba::this_coro::executor, 250ms);
                co_await timer.async_wait(ba::use_awaitable);

                try {
                    co_await enterConfigMode();
                } catch (const std::exception &e) {
                    throw;
                }

                // Validate baud rate
                command = buildGetCommand(parsedOption);
                response = co_await sendAtCommand(command, msCONFIG_TIMEOUT);

                currentBaudRate = mpUart->getBaudRate();
                mpLogger->debugf("[HC12_DRIVER] [SET_OPTION] changed baudrate %d", currentBaudRate);
            }

            const auto responseValue = parseResponse(response);

            // Extract values
            const std::string responseValueStr = {responseValue.begin(), responseValue.end()};
            const std::string responseStr = {response.begin(), response.end()};

            // Validate response, with spacial case for resetting to default
            if (value == responseValueStr ||
                (parsedOption == Hc12Option::DEFAULT && responseStr == RfTypes::DEFAULT_STRING)) {
                success = true;
                mpLogger->debugf("[HC12_DRIVER] [SET_OPTION] option set: %s=%s",
                                 option.data(), responseValueStr.c_str());
            } else {
                sprintf(buffer, "set failed, expected value: %s, received: %s",
                        value.c_str(), responseValueStr.c_str());
                throw std::runtime_error(buffer);
            }
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] [SET_OPTION] sendCommand error: %s", e.what());
            // Set failed - restore baud rate
            if (currentBaudRate > 0) {
                mpUart->setBaudRate(currentBaudRate);
            }
            throw;
        }

        // Exit config mode
        try {
            co_await exitConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] [SET_OPTION] exitConfig error: %s", e.what());
            throw;
        }

        // Update cache on success
        if (success && parsedOption != Hc12Option::DEFAULT) {
            mOptionsCache[std::string(option)] = std::string(value);
        }

        co_return success;
    }

    ba::awaitable<std::vector<uint8_t> > HC12Driver::getOption(const std::string option) {
        // Try cache first
        const auto iter = mOptionsCache.find(std::string(option));
        if (iter != mOptionsCache.end()) {
            mpLogger->debugf("[HC12_DRIVER] [GET_OPTION] cache found: %s=%s",
                             option.data(), iter->second.c_str());
            std::vector<uint8_t> result = {iter->second.begin(), iter->second.end()};
            co_return result;
        }

        // Convert to enum for validation
        const Hc12Option opt = stringToOption(option);
        if (opt == Hc12Option::UNDEFINED) {
            throw std::invalid_argument("getOption error: unknown option");
        }

        std::vector<uint8_t> value;

        // Enter config mode - to send RY command
        try {
            co_await enterConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] [GET_OPTION] enterConfig error: %s", e.what());
            throw;
        }

        // Query HC-12
        try {
            const auto command = buildGetCommand(opt);
            const auto response = co_await sendAtCommand(command, msCONFIG_TIMEOUT);
            value = parseResponse(response);

            const std::string valueStr = {value.begin(), value.end()};

            if (!value.empty()) {
                mpLogger->debugf("[HC12_DRIVER] [GET_OPTION] cached: %s=%s",
                                 option.c_str(), valueStr.c_str());
            }
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] [GET_OPTION] sendCommand error: %s", e.what());
            throw;
        }

        // Exit config mode
        try {
            co_await exitConfigMode();
        } catch (const std::exception &e) {
            mpLogger->errorf("[HC12_DRIVER] [GET_OPTION] exitConfig error: %s", e.what());
            throw;
        }

        // Update cache on successful query
        if (!value.empty()) {
            mOptionsCache[std::string(option)] = {value.begin(), value.end()};
        }

        co_return value;
    }

    ba::awaitable<std::string> HC12Driver::getAllOptions() {
        std::string formatted;

        const auto options = {Hc12Option::CHANNEL, Hc12Option::BAUDRATE, Hc12Option::FU_MODE, Hc12Option::POWER};

        for (const auto &option: options) {
            const auto optionStr = optionToString(option);
            formatted += optionStr + ':';
            try {
                const auto response = co_await getOption(optionStr);
                formatted.append(response.begin(), response.end());
            } catch (const std::exception &e) {
                mpLogger->errorf("[HC12_DRIVER] [GET_ALL_OPTIONS] getOption (%s) error: %s",
                                 optionStr.c_str(),
                                 e.what());
                formatted += "error_value";
            }
            formatted += ',';
        }
        formatted.pop_back();

        co_return formatted;
    }

    std::chrono::milliseconds HC12Driver::getRequiredWriteDelay() {
        // Check FU mode from cache
        const auto iter = mOptionsCache.find("fu_mode");
        if (iter != mOptionsCache.end()) {
            const std::string &mode = iter->second;
            if (mode == "2" || mode == "4") {
                return msHIGH_WRITE_DELAY;
            }
        }

        return msLOW_WRITE_DELAY;
    }

    bool HC12Driver::isMultiChannel() {
        return msIsMultiChannel;
    }

    ba::awaitable<void> HC12Driver::enterConfigMode() {
        if (mIsInConfigMode.load(std::memory_order::acquire)) {
            co_return;
        }

        // Set pin LOW to enter config mode, runtime_error on failure.
        mpSetPin->setLow();

        // Wait 40ms as per HC-12 specification
        ba::steady_timer timer(co_await ba::this_coro::executor, 40ms);
        co_await timer.async_wait(ba::use_awaitable);

        mIsInConfigMode.store(true, std::memory_order::release);
        mpLogger->debugf("[HC12_DRIVER] Entered config mode");
    }

    ba::awaitable<void> HC12Driver::exitConfigMode() {
        if (!mIsInConfigMode.load(std::memory_order::acquire)) {
            co_return;
        }

        // Set pin HIGH to exit config mode, runtime_error on failure.
        mpSetPin->setHigh();

        // Wait 80ms as per HC-12 specification, with additional margin
        ba::steady_timer timer(co_await ba::this_coro::executor, 80ms);
        co_await timer.async_wait(ba::use_awaitable);


        mIsInConfigMode.store(false, std::memory_order::release);
        mpLogger->debugf("[HC12_DRIVER] Exited config mode");
    }

    ba::awaitable<std::vector<uint8_t> > HC12Driver::sendAtCommand(std::vector<uint8_t> command,
                                                                   const std::chrono::milliseconds timeout) const {
        co_await mpUart->writeAsync(command);

        auto response = co_await mpUart->readUntil(timeout);

        std::string commandStr = {command.begin(), command.end()};
        std::string responseStr = {response.begin(), response.end()};

        mpLogger->debugf("[HC12_DRIVER] AT command: %s -> %s", commandStr.c_str(), responseStr.c_str());

        co_return response;
    }

    auto HC12Driver::stringToOption(const std::string_view option) -> Hc12Option {
        if (option == "channel") return Hc12Option::CHANNEL;
        if (option == "baudrate") return Hc12Option::BAUDRATE;
        if (option == "fu_mode") return Hc12Option::FU_MODE;
        if (option == "power") return Hc12Option::POWER;
        if (option == "default") return Hc12Option::DEFAULT;
        return Hc12Option::UNDEFINED;
    }

    std::string HC12Driver::optionToString(const Hc12Option option) {
        switch (option) {
            case Hc12Option::CHANNEL: return "channel";
            case Hc12Option::BAUDRATE: return "baudrate";
            case Hc12Option::FU_MODE: return "fu_mode";
            case Hc12Option::POWER: return "power";
            case Hc12Option::DEFAULT: return "default";
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
            case Hc12Option::DEFAULT:
                if (value != RfTypes::ALL_OPTIONS_STRING) break;
                commandStr = "AT+DEFAULT";
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

        // Accept response even if first char is missing
        if (!(responseStr.starts_with("OK") || responseStr.starts_with("K"))) {
            return {};
        }

        // Special case for power response
        if (responseStr.find("RP:") != std::string_view::npos) {
            // Extract dBm value and convert to level
            auto dbmPos = responseStr.find_last_of("+-");

            if (dbmPos != std::string_view::npos) {
                dbmPos = dbmPos + 1; // Shift position to first number of dBm

                // Find end of number (first non-digit character)
                auto dbmEnd = responseStr.find_first_not_of("0123456789", dbmPos);
                if (dbmEnd == std::string_view::npos) {
                    dbmEnd = responseStr.length();
                }

                const std::string dbmStr(responseStr.substr(dbmPos, dbmEnd - dbmPos));
                const std::string powerLevel = dbmToPowerLevel(dbmStr);

                return {powerLevel.begin(), powerLevel.end()};
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

        // Handle reset to default settings
        if (valuePart == RfTypes::DEFAULT_STRING) {
            return {valuePart.begin(), valuePart.end()};
        }

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

    std::string HC12Driver::dbmToPowerLevel(const std::string_view dbm) {
        const auto iter = std::ranges::find(msDBM_ARRAY, dbm);
        if (iter != msDBM_ARRAY.end()) {
            const size_t index = std::distance(msDBM_ARRAY.begin(), iter);

            return std::to_string(index + 1);
        }
        return "";
    }

    bool HC12Driver::isChannelValid(const int channel) {
        return channel >= msMIN_CHANNEL && channel <= msMAX_CHANNEL;
    }
}
