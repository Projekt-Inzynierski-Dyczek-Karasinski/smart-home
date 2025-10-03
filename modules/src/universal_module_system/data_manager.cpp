#include "data_manager.h"

#include <SPIFFS.h>

namespace UniversalModuleSystem {
    DataManager &DataManager::getInstance() {
        static DataManager instance;
        return instance;
    }

    DataManager::DataManager() {
        mFileAccessMutex = xSemaphoreCreateMutex();
        SPIFFS.begin(true);
    };

    DataManager::~DataManager() {
        vSemaphoreDelete(mFileAccessMutex);
        SPIFFS.end();
    }

    void DataManager::save(const char *path, nl::json data) const {
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        File file = SPIFFS.open(path, "w");
        file.println(data.dump().c_str());
        file.close();
        xSemaphoreGive(mFileAccessMutex);
    }

    nl::json DataManager::load(const char *path) const {
        nl::json result;
        if (SPIFFS.exists(path)) {
            xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
            File file = SPIFFS.open(path, "r");
            result = nl::json::parse(file.readString());
            file.close();
            xSemaphoreGive(mFileAccessMutex);
        }
        return result;
    }

    bool DataManager::isFileExists(const char *path) {
        return SPIFFS.exists(path);
    }

    void DataManager::eraseAllData() const {
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        SPIFFS.format();
        xSemaphoreGive(mFileAccessMutex);
    }
}