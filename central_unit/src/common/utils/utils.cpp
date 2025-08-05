#include "utils.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <utility>

#include <sys/file.h>

namespace SmartHome::Utils {
    ServiceType resolveServiceType(const std::string &typeStr) {
        if (typeStr == "STANDALONE") {
            return ServiceType::STANDALONE;
        }

        if (typeStr == "SYSTEMD") {
            return ServiceType::SYSTEMD;
        }

        return ServiceType::AUTO;
    }

    FileLock::FileLock(const std::string &lockFilePath) {
        mLockFilePath = lockFilePath;

        // Check if lock file exists, and if process that created it is currently running.
        if (std::filesystem::exists(mLockFilePath)) {
            const auto oldPid = readPidFromFile();
            if (oldPid.has_value() && isProcessRunning(oldPid.value())) {
                throw std::runtime_error("Another instance is already running PID " + std::to_string(oldPid.value()));
            }

            std::error_code ec;
            std::filesystem::remove(mLockFilePath, ec);
            if (ec) {
               throw std::runtime_error("Failed to remove old lock file. Try: sudo rm " + mLockFilePath);
            }
        }

        // Create new lock file or lock already existing file
        mLockFd = open(mLockFilePath.c_str(), O_RDWR | O_CREAT, 0666);

        if (mLockFd < 0) throw std::runtime_error("Failed to open lock file (" + mLockFilePath + ")");
        if (flock(mLockFd, LOCK_EX | LOCK_NB) != 0) {
           close(mLockFd);
            throw std::runtime_error("Failed to lock file (" + mLockFilePath + ")");
        }
        if (!writePidToFile()) {
            throw std::runtime_error("Failed to write pid to file (" + mLockFilePath + ")");
        }
    }

    FileLock::~FileLock() {
        if (mLockFd >= 0) {
            flock(mLockFd, LOCK_UN);
            close(mLockFd);
            if (std::filesystem::exists(mLockFilePath)) {
                // Remove lock file to avoid file permission problems
                std::remove(mLockFilePath.c_str());
            }
        }
    }

    bool FileLock::writePidToFile() const {
        if (ftruncate(mLockFd, 0) != 0) {
            return false;
        }
        const std::string pidStr = std::to_string(getpid()) + "\n";
        return write(mLockFd, pidStr.c_str(), pidStr.length()) == pidStr.length();
    }

    std::optional<pid_t> FileLock::readPidFromFile() const {
        std::ifstream file(mLockFilePath);
        pid_t pid;
        if (file >> pid) {
            return std::make_optional(pid); // Return PID on successful read
        }
        return std::nullopt; // nullopt on read fail
    }

    bool FileLock::isProcessRunning(const pid_t &pid) {
        return kill(pid, 0) == 0 || errno == EPERM;
    }
}
