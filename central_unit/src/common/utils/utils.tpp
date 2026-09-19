#pragma once

namespace SmartHome::Utils {
    template<typename T>
    void failPromise(const std::shared_ptr<std::promise<T> > &promise, std::string_view reason) {
        if (!promise) return;
        try {
            promise->set_exception(std::make_exception_ptr(std::runtime_error(reason.data())));
        } catch (...) {
            // Already satisfied
        }
    }
}
