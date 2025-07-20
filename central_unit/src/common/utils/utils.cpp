#include "utils.h"

#include <fcntl.h>
#include <stdexcept>

#include <sys/file.h>
#include <boost/asio/detail/descriptor_ops.hpp>
#include <boost/process/io.hpp>

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
        mLockFd = open(lockFilePath.c_str(), O_RDWR | O_CREAT, 0666);

        if (mLockFd < 0) throw std::runtime_error("Failed to open lock file (" + lockFilePath + ")");
        if (flock(mLockFd, LOCK_EX | LOCK_NB) != 0) {
            close(mLockFd);
            throw std::runtime_error("Failed to lock file (" + lockFilePath + ")");
        }
        writePidToFile();
    }

    FileLock::~FileLock() {
        if (mLockFd >= 0) {
            flock(mLockFd, LOCK_UN);
            close(mLockFd);
        }
    }

    bool FileLock::writePidToFile() const {
        if (ftruncate(mLockFd, 0) != 0) {
            return false;
        }
        const std::string pidStr = std::to_string(getpid()) + "\n";
        return write(mLockFd, pidStr.c_str(), pidStr.length()) == pidStr.length();
    }
}
