#include "gui.h"
#include "exceptions.hpp"

namespace se = SmartHome::Exceptions;

namespace SmartHomeGui {
    Gui::Gui() {
    }

    bool Gui::initialize(const std::shared_ptr<su::Logger> &logger) {
        if (mIsInitialized) {
            logger->error("[GUI] GUI already initialized");
            return false;
        }

        try {
            lockFile.emplace(ms_LOCK_FILE_PATH.data());
        } catch (const se::Exception &e) {
            if (e.getCode() == se::ExceptionCodes::INSTANCE_ALREADY_EXISTS) {
                logger->info("[GUI] Instance already exists");
                const pid_t pid = su::FileLock::readPidFromFile(ms_LOCK_FILE_PATH.data()).value_or(-1);

                if (pid == -1) {
                    logger->error("[GUI] Failed to read pid from lock file");
                    return false;
                }

                if (!su::FileLock::isProcessRunning(pid)) {
                    logger->error("[GUI] Existing instance is not running");
                    return false;
                }

                logger->info("Sending SIGUSR1 to running instance");
                kill(pid, SIGUSR1);
            } else {
                logger->errorf("[GUI] Lock file error: %s", e.what());
            }
            return false;
        } catch (const std::exception &e) {
            logger->errorf("[GUI] Unexpected error: %s", e.what());
            return false;
        }

        mpLogger = std::make_unique<su::AsyncLogger>(logger, mUtilityIoContext);
        logger->enableFileLoggingIfConfigured();

        // Create thread running io context for signal handling and logging
        mUtilityGuard.emplace(ba::make_work_guard(mUtilityIoContext));
        mUtilityThread = std::thread([this] {
            mUtilityIoContext.run();
        });

        mMainGuard.emplace(ba::make_work_guard(mMainIoContext));
        mMainThread = std::thread([this] {
            mMainIoContext.run();
        });

        mIsInitialized = true;
        logger->debug("[GUI] GUI successfully initialized");
        return true;
    }

    void Gui::run() {
        if (!mIsInitialized) {
            mpLogger->error("[GUI] GUI not initialized");
            return;
        }
        if (mIsRunning) {
            mpLogger->error("[GUI] GUI already running");
            return;
        }

        mIsRunning = true;
        mpLogger->debug("[GUI] GUI running");

        mSignals.emplace(mUtilityIoContext);
        for (const auto &sig: ms_SIGNALS_TO_HANDLE) {
            mSignals->add(sig);
        }

        mSignals->async_wait([this](const bs::error_code &ec, const int sig) {
            signalHandler(ec, sig);
        });

        mMainThread->join();
        mMainGuard.reset();

        //TODO stop utility
    }
}
