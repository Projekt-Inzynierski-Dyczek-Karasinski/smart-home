#include "data_manager.h"

#include <SPIFFS.h>

#include "../config/universal_module_system_config.h"
#include "../config/addressing_config.h"
#include "../config/ota_config.h"

namespace nl = nlohmann;

namespace UniversalModuleSystem {
    DataManager &DataManager::getInstance(const std::shared_ptr<ul::Logger> &logger) {
        static DataManager instance(logger);
        return instance;
    }

    DataManager::DataManager(const std::shared_ptr<ul::Logger> &logger) : mpLogger(logger) {
        if (logger == nullptr) {
            mpLogger = std::make_shared<ul::Logger>();
            mpLogger->error("DataManager", "DataManager's constructor didn't get pointer to logger instance.");
        }
        mFileAccessMutex = xSemaphoreCreateMutex();
        SPIFFS.begin(true);
        loadBaseConfig(false);
        mpLogger->verbose("DataManager", "DataManager initialized.");
    };

    DataManager::~DataManager() {
        vSemaphoreDelete(mFileAccessMutex);
        SPIFFS.end();
    }

    void DataManager::saveJson(const char *path, const nl::json &data) const {
        if (strncmp(path, ms_ROOT_PATH, strlen(ms_ROOT_PATH)) != 0) {
            mpLogger->error("DataManager", "Files not in /root are read only.");
            return;
        }

        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        File file = SPIFFS.open(path, "w");
        file.println(data.dump().c_str());
        file.close();
        xSemaphoreGive(mFileAccessMutex);
    }

    nl::json DataManager::loadJson(const char *path) const {
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
    
    bool DataManager::fileExists(const char *path) const {
        return SPIFFS.exists(path);
    }

    void DataManager::ls() const {
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);

        File root = SPIFFS.open(ms_ROOT_PATH);
        File file = root.openNextFile();
        while(file) {
            mpLogger->info("DataManager ls", file.path());
            file.close();
            file = root.openNextFile();
        }

        xSemaphoreGive(mFileAccessMutex);
    }

    void DataManager::cat(const char *path) const {
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        if (!SPIFFS.exists(path)) {
            xSemaphoreGive(mFileAccessMutex);
            mpLogger->error("DataManager cat", "File does not exists.");
            return;
        }
        File file = SPIFFS.open(path, "r");
        const String data = file.readString();
        file.close();
        xSemaphoreGive(mFileAccessMutex);

        const size_t len = data.length() + 1;
        char charData[len];
        data.toCharArray(charData, len);
        mpLogger->info("DataManager cat", charData);
    }

    void DataManager::rm(const char *path) const {
        if (strncmp(path, ms_ROOT_PATH, strlen(ms_ROOT_PATH)) != 0) {
            mpLogger->error("DataManager rm", "Files not in /root are read only.");
            return;
        }

        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        SPIFFS.remove(path);
        xSemaphoreGive(mFileAccessMutex);
        mpLogger->info("DataManager rm", "Removed file.");
    }

    void DataManager::loadBaseConfig(const bool overrideExistingFiles) const {
        File root = SPIFFS.open(ms_ROOT_PATH);
        if (overrideExistingFiles) {
            File file = root.openNextFile();
            while(file) {
                SPIFFS.remove(file.path());
                file = root.openNextFile();
            }
        }

        // logger
        nl::json baseConfig = loadJson(s_BASE_CONFIG_PATH);
        if (baseConfig.empty()) {
            mpLogger->error("DataManager", "Base config not found!");
            return;
        }

        // add here other files to create
        if (overrideExistingFiles || !SPIFFS.exists(ADDRESSING_DATA_PATH)) {
            saveJson(ADDRESSING_DATA_PATH, baseConfig["addressing"]);
        }
        if (overrideExistingFiles || !SPIFFS.exists(WIFI_DATA_PATH)) {
            saveJson(WIFI_DATA_PATH, baseConfig["wifi"]);
        }
    }

    void DataManager::waitAndDisable() const {
        xSemaphoreTake(mFileAccessMutex, pdMS_TO_TICKS(POWER_MANAGEMENT_SEMAPHORE_TIMEOUT));
    }
}