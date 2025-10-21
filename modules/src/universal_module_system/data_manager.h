#pragma once

#include <Arduino.h>
#include <nlohmann/json.hpp>

#include "utils/logger.h"

namespace ul = Utils::Logging;
// TODO !pr update comments

namespace UniversalModuleSystem {
    /**
     * @brief Singleton class responsible for managing data stored in flash memory.
     * Provides thread-safe file operations for saving and loading JSON data from/to the ESP32's SPIFFS file system.
     */
    class DataManager {
    public:
        /**
         * @brief Gets the singleton instance of DataManager.
         *
         * @param logger Shared pointer to the logger instance, default: nullptr.
         * @return Reference to the DataManager instance.
         *
         * @warning First call have to pass pointer to logger.
         */
        static DataManager& getInstance(const std::shared_ptr<ul::Logger> &logger = nullptr);

        // Delete copy constructor and assignment operator
        DataManager(const DataManager&) = delete;
        DataManager& operator = (const DataManager&) = delete;

        /**
         * @brief Saves JSON data to a file in SPIFFS.
         *
         * @param path File path where data will be saved\n
         * (have to start with <b>/</b>).
         * @param data JSON object containing the data to save.
         *
         * @warning If file passed in <code>path</code> exists, it will be overwritten.
         * @note Thread-safe.
         */
        void saveJson(const char *path, const nlohmann::json &data) const;

        /**
         * @brief Loads JSON data from a file in SPIFFS.
         *
         * @param path File path in SPIFFS to load data from.
         * @return JSON object containing the loaded data (empty JSON object if file doesn't exist).
         *
         * @note Thread-safe.
         */
        nlohmann::json loadJson(const char *path) const;

        /**
         * @brief Checks if a file exists in SPIFFS.
         *
         * @param path File path to check \n
         * (have to start with <b>/</b>).
         * @return True if the file exists, false otherwise.
         */
        bool fileExists(const char *path) const;

        /**
         * @brief Prints files in /root directory (like "ls" linux command).
         * @note Thread-safe.
         */
        void ls() const;

        /**
         * @brief Prints content of file (like "cat" linux command).
         *
         * @param path Path to file.\n
         * (have to start with <b>/</b>).
         *
         * @note Thread-safe.
         */
        void cat(const char* path) const;

        /**
        * @brief Removes file from SPIFFS.
        *
        * @param path Path to file.\n
        * (have to start with <b>/</b>).
        *
        * @note Thread-safe.
        */
        void rm(const char* path) const;

        /**
         * @brief Loads <b>read only</b> base config.
         * @details If <code>overrideExistingFiles</code> is set to true it will delete/override existing files,\n
         *          otherwise loads base config only if config files don't exist.
         *
         * @param overrideExistingFiles True for delete/override existing files, default: false.
         */
        void loadBaseConfig(bool overrideExistingFiles = false);

        /**
        * @brief Waits for ongoing flash data modification to complete and disables it.
        */
        void waitAndDisable() const;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         * Initializes mutex protecting access to files and mounts the SPIFFS file system.
         *
         * @note Automatically format SPIFFS file system if mounts fail.
         */
        explicit DataManager(const std::shared_ptr<ul::Logger> &logger);

        /**
         * @brief Private destructor for singleton pattern
         * Deletes mutex and unmounts the SPIFFS file system.
         */
        ~DataManager();

        std::shared_ptr<ul::Logger> mpLogger;

        SemaphoreHandle_t mFileAccessMutex = nullptr; ///< FreeRTOS mutex protecting access to files.

        static constexpr char ms_ROOT_PATH[] = "/root"; ///< Prefix for all files stored in SPIFFS due to issues with opening root ("/") directory
        static constexpr char ms_BASE_CONFIG_PATH[] = "/base_config.json"; ///< Path to config uploaded by platformio
    };
}