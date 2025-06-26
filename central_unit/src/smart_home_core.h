#ifndef SMART_HOME_CORE_H
#define SMART_HOME_CORE_H
#include <atomic>

namespace SmartHome {
    class Core {
    public:
        static Core &Instance();

        Core(const Core &) = delete;

        Core &operator=(const Core &) = delete;

        bool initialize();

        void run();

        void shutdown();

    private:
        Core();

        ~Core();

        std::atomic<bool> mpInitialized{false};
        std::atomic<bool> mpRunning{false};
    };
}

#endif //SMART_HOME_CORE_H
