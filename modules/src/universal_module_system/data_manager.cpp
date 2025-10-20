#include "data_manager.h"

#include <SPIFFS.h>

#include "../../config/common/freertos_common_config.h"
#include "utils/logger.h"

namespace nl = nlohmann;
namespace ul = Utils::Logging;

namespace UniversalModuleSystem {
    DataManager &DataManager::getInstance() {
        static DataManager instance;
        return instance;
    }

    DataManager::DataManager() {
        mFileAccessMutex = xSemaphoreCreateMutex();
        SPIFFS.begin(true);
        // TODO !pr first start of DataManager should check if only base config files exists -> if yes, create copy of them and only copy edit
        // TODO !pr consider making base_config.json next to (not inside) /root, with dedicated read method
    };

    DataManager::~DataManager() {
        vSemaphoreDelete(mFileAccessMutex);
        SPIFFS.end();
    }

    void DataManager::save(const char *path, const nl::json &data) const {
        if (strncmp(path, ms_BASE_CONFIG_PATH, strlen(ms_BASE_CONFIG_PATH)) == 0) {
            ul::Logger logger;
            logger.error("DataManager rm", "This file is read only.");
            return;
        }
        size_t pathLen = snprintf(nullptr, 0, "%s%s", ms_ROOT_PATH, path);
        char newPath[pathLen];
        sprintf(newPath, "%s%s", ms_ROOT_PATH, path);

        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        File file = SPIFFS.open(newPath, "w");
        file.println(data.dump().c_str());
        file.close();
        xSemaphoreGive(mFileAccessMutex);
    }

    nl::json DataManager::load(const char *path) const {
        size_t pathLen = snprintf(nullptr, 0, "%s%s", ms_ROOT_PATH, path);
        char newPath[pathLen];
        sprintf(newPath, "%s%s", ms_ROOT_PATH, path);

        nl::json result;
        if (SPIFFS.exists(newPath)) {
            xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
            File file = SPIFFS.open(newPath, "r");
            result = nl::json::parse(file.readString());
            file.close();
            xSemaphoreGive(mFileAccessMutex);
        }
        return result;
    }

    bool DataManager::fileExists(const char *path) const {
        size_t pathLen = snprintf(nullptr, 0, "%s%s", ms_ROOT_PATH, path);
        char newPath[pathLen];
        sprintf(newPath, "%s%s", ms_ROOT_PATH, path);

        return SPIFFS.exists(newPath);
    }

    void DataManager::ls(const char *path) const {
        size_t pathLen = snprintf(nullptr, 0, "%s%s", ms_ROOT_PATH, path);
        char newPath[pathLen];
        sprintf(newPath, "%s%s", ms_ROOT_PATH, path);

        ul::Logger logger;
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);

        File root = SPIFFS.open(newPath);
        File file = root.openNextFile();
        while(file) {
            logger.info("DataManager ls", file.name());
            file.close();
            file = root.openNextFile();
        }

        xSemaphoreGive(mFileAccessMutex);
    }

    void DataManager::cat(const char *path) const {
        size_t pathLen = snprintf(nullptr, 0, "%s%s", ms_ROOT_PATH, path);
        char newPath[pathLen];
        sprintf(newPath, "%s%s", ms_ROOT_PATH, path);

        ul::Logger logger;
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        if (!SPIFFS.exists(newPath)) {
            xSemaphoreGive(mFileAccessMutex);
            logger.warning("DataManager cat", "File does not exists.");
            return;
        }

        File file = SPIFFS.open(newPath, "r");
        const String data = file.readString();
        file.close();
        xSemaphoreGive(mFileAccessMutex);

        const size_t len = data.length() + 1;
        char charData[len];
        data.toCharArray(charData, len);
        logger.info("DataManager cat", charData);
    }

    void DataManager::rm(const char *path) {
        if (strncmp(path, ms_BASE_CONFIG_PATH, strlen(ms_BASE_CONFIG_PATH)) == 0) {
            ul::Logger logger;
            logger.error("DataManager rm", "This file is read only.");
            return;
        }
        size_t pathLen = snprintf(nullptr, 0, "%s%s", ms_ROOT_PATH, path);
        char newPath[pathLen];
        sprintf(newPath, "%s%s", ms_ROOT_PATH, path);

        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        SPIFFS.remove(newPath);
        xSemaphoreGive(mFileAccessMutex);
    }


    void DataManager::eraseAllData() const {
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        SPIFFS.format(); // TODO !pr change to remove files
        xSemaphoreGive(mFileAccessMutex);
    }

    void DataManager::waitAndDisable() const {
        xSemaphoreTake(mFileAccessMutex, pdMS_TO_TICKS(POWER_MANAGEMENT_SEMAPHORE_TIMEOUT));
    }
}