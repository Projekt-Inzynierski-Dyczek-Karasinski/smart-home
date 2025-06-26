#include "smart_home_core.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace SmartHome {
    Core &Core::Instance() {
        static Core CoreInstance;
        return CoreInstance;
    }

    Core::Core() {
    }

    Core::~Core() {
        if (mpRunning.load()) {
            shutdown();
        }
    }

    bool Core::initialize() {
        if (mpInitialized.load()) {
            std::cerr << "Core already initialized" << std::endl;
            return false;
        }

        std::cout << "Initializing core" << std::endl;

        //TODO load config
        //TODO handle config

        mpInitialized.store(true);
        std::cout << "Initialization complete" << std::endl;

        return true;
    }

    void Core::run() {
        if (mpInitialized.load() == false) {
            std::cerr << "Core not initialized" << std::endl;
            return;
        } else if (mpRunning.load() == true) {
            std::cerr << "Core already running" << std::endl;
            return;
        }

        mpRunning.store(true);
        std::cout << "Running core" << std::endl;

        int test = 0; //TODO delete
        while (mpRunning.load() == true) {
            std::cout << "Running core - " << test++ << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (test == 10) {
                shutdown();
            }
        }

        std::cout << "Done running core" << std::endl;
    }

    void Core::shutdown() {
        std::cout << "Shutting down core" << std::endl;

        mpRunning.store(false);

        std::cout << "Shutdown complete" << std::endl;
    }
}
