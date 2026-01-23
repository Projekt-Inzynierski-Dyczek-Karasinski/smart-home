#include "gpio_handler.h"

#include <stdexcept>


namespace SmartHomeMediator {
    GpioHandler::GpioHandler(const int pin,
                             const bool setInitialStateHigh,
                             const bool setPinToOutput,
                             const std::string &chipName)
        : mChip(chipName), mPin(pin) {
        try {
            // Get GPIO line
            mLine = mChip.get_line(pin);

            // Request line as output with initial value
            mLine.request({
                              "smarthome-mediator",
                              setPinToOutput
                                  ? gpiod::line_request::DIRECTION_OUTPUT
                                  : gpiod::line_request::DIRECTION_INPUT,
                              0
                          },
                          setInitialStateHigh ? 1 : 0);
        } catch (const std::exception &e) {
            throw std::runtime_error(
                "Failed to initialize GPIO pin " + std::to_string(pin) + ": " + e.what()
            );
        }
    }

    void GpioHandler::setHigh() const {
        try {
            mLine.set_value(1);
        } catch (const std::exception &e) {
            throw std::runtime_error(
                "Failed to set GPIO pin " + std::to_string(mPin) + " HIGH: " + e.what()
            );
        }
    }

    void GpioHandler::setLow() const {
        try {
            mLine.set_value(0);
        } catch (const std::exception &e) {
            throw std::runtime_error(
                "Failed to set GPIO pin " + std::to_string(mPin) + " LOW: " + e.what()
            );
        }
    }

    int GpioHandler::read() const {
        try {
            return mLine.get_value();
        } catch (const std::exception &e) {
            throw std::runtime_error(
                "Failed to read GPIO pin " + std::to_string(mPin) + ": " + e.what()
            );
        }
    }
}
