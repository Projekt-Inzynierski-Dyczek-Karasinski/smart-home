#include "utils.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <csignal>
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

    std::chrono::system_clock::time_point parseTimestampTz(const std::string &timestamp) {
        std::tm tm;
        int tzOffsetHours = 0;

        sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        // Adjust tm structure for timegm
        tm.tm_year -= 1900; // tm_year is years since 1900
        tm.tm_mon -= 1; // tm_mon is 0-based

        // Check for timezone offset in format ±HH at the end of the string
        const auto pos = timestamp.find_last_of("+-");
        // pos > 10 to ensure offset is after date and time
        if (pos != std::string_view::npos && pos > 10) {
            try {
                tzOffsetHours = std::stoi(timestamp.substr(pos));
            } catch (...) {
                // If parsing fails, default to 0 offset
            }
        }

        auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));
        tp -= std::chrono::hours(tzOffsetHours); // Adjust for timezone offset

        return tp;
    }

    std::string timePointToTimestampTz(const std::chrono::system_clock::time_point &timePoint) {
        const auto timeT = std::chrono::system_clock::to_time_t(timePoint);

        std::tm tmUTC;
        gmtime_r(&timeT, &tmUTC);

        std::ostringstream oss;
        oss << std::put_time(&tmUTC, "%Y-%m-%dT%H:%M:%SZ");

        return oss.str();
    }

    FileLock::FileLock(const std::string &lockFilePath) {
        mLockFilePath = lockFilePath;

        // Check if lock file exists, and if process that created it is currently running.
        if (std::filesystem::exists(mLockFilePath)) {
            const auto oldPid = readPidFromFile(mLockFilePath);
            if (oldPid.has_value() && isProcessRunning(oldPid.value())) {
                throw Exceptions::Exception(Exceptions::ExceptionCodes::INSTANCE_ALREADY_EXISTS,
                                            "Another instance is already running PID " + std::to_string(
                                                oldPid.value()));
            }

            std::error_code ec;
            std::filesystem::remove(mLockFilePath, ec);
            if (ec) {
                throw Exceptions::Exception(Exceptions::ExceptionCodes::FILE_LOCK_REMOVE_FAILED,
                                            "Failed to remove old lock file. Try: sudo rm " + mLockFilePath);
            }
        }

        // Create new lock file or lock already existing file
        mLockFd = open(mLockFilePath.c_str(), O_RDWR | O_CREAT, 0666);

        if (mLockFd < 0)
            throw Exceptions::Exception(Exceptions::ExceptionCodes::FILE_LOCK_OPEN_FAILED,
                                        "Failed to open lock file (" + mLockFilePath + ")");
        if (flock(mLockFd, LOCK_EX | LOCK_NB) != 0) {
            close(mLockFd);
            throw Exceptions::Exception(Exceptions::ExceptionCodes::FILE_LOCK_FAILED,
                                        "Failed to lock file (" + mLockFilePath + ")");
        }
        if (!writePidToFile()) {
            throw Exceptions::Exception(Exceptions::ExceptionCodes::FILE_LOCK_WRITE_FAILED,
                                        "Failed to write pid to file (" + mLockFilePath + ")");
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

    std::optional<pid_t> FileLock::readPidFromFile(const std::string_view lockFilePath) {
        std::ifstream file(lockFilePath.data());
        pid_t pid;
        if (file >> pid) {
            return std::make_optional(pid); // Return PID on successful read
        }
        return std::nullopt; // nullopt on read fail
    }

    bool FileLock::isProcessRunning(const pid_t &pid) {
        return kill(pid, 0) == 0 || errno == EPERM;
    }

    bool FileLock::writePidToFile() const {
        if (ftruncate(mLockFd, 0) != 0) {
            return false;
        }
        const std::string pidStr = std::to_string(getpid()) + "\n";
        return write(mLockFd, pidStr.c_str(), pidStr.length()) == pidStr.length();
    }
}
