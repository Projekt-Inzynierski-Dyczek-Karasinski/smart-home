#pragma once

#include <Arduino.h>
#include <memory>
#include <nlohmann/json.hpp>

#include "utils/logger.h"

namespace ul = Utils::Logging;
namespace nl = nlohmann;

namespace UniversalModuleSystem {
    class DataManager {
    public:
        static DataManager& getInstance();

        // Delete copy constructor and assignment operator
        DataManager(const DataManager&) = delete;
        DataManager& operator = (const DataManager&) = delete;

        void save(const char *path, nl::json data) const;
        nl::json load(const char *path) const;

        bool isFileExists(const char *path);

        void eraseAllData() const;

    private:
        DataManager();
        ~DataManager();

        SemaphoreHandle_t mFileAccessMutex = nullptr;
    };
}