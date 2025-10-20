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
    };

    DataManager::~DataManager() {
        vSemaphoreDelete(mFileAccessMutex);
        SPIFFS.end();
    }

    void DataManager::save(const char *path, const nl::json &data) const {
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

    void DataManager::cat(const char *path) {
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

    void DataManager::eraseAllData() const {
        xSemaphoreTake(mFileAccessMutex, portMAX_DELAY);
        SPIFFS.format();
        xSemaphoreGive(mFileAccessMutex);
    }

    void DataManager::waitAndDisable() const {
        xSemaphoreTake(mFileAccessMutex, pdMS_TO_TICKS(POWER_MANAGEMENT_SEMAPHORE_TIMEOUT));
    }
}