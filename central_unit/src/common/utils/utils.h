#pragma once

#include <string>
#include <stdexcept>

namespace SmartHome::Utils {
    /**
     * @brief Service management type enum.
     *
     * @details Defines in what mode services should be managed:
     *              - AUTO: Automatically detects based on environment
     *              - STANDALONE: Run as independent process with file locking
     *              - SYSTEMD: Integrate with systemd service manager
     */
    enum class ServiceType {
        AUTO, ///< Automatically detects based on environment
        STANDALONE, ///< Run as independent process with file locking
        SYSTEMD ///< Integrate with systemd service manager
    };

    /**
     * @brief Convert string to ServiceType enum value.
     *
     * @param typeStr String representation of service type.
     * @return Corresponding ServiceType enum value, defaults to AUTO.
     */
    ServiceType resolveServiceType(const std::string &typeStr);

    /**
     * @brief RAII wrapper for exclusive file locking.
     *
     * @details Acquires exclusive lock on specified file using flock(). Lock is automatically released on destruction.
     */
    class FileLock {
    public:
        /**
         * @brief Construct and acquire lock on file.
         *
         * @param lockFilePath Path to lock file.
         * @throw std::runtime_error if lock cannot be created or locked.
         */
        explicit FileLock(const std::string &lockFilePath);

        /**
         * @brief Release lock and close file descriptor.
         */
        ~FileLock();

        // Prevent copy and move
        FileLock(const FileLock &) = delete;

        FileLock &operator=(const FileLock &) = delete;

        FileLock(FileLock &&) = delete;

        FileLock &operator=(FileLock &&) = delete;

    private:
        /**
         * @brief Writes PID to lock file.
         *
         * @return true if write operation was successful, false otherwise.
         */
        bool writePidToFile() const;

        int mLockFd = -1; ///< File descriptor for lock file
    };
}
