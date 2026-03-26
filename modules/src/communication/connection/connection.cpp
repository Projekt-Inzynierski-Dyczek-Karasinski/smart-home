#include "connection.h"

#include "communication/communication.h"
#include "universal_module_system/power_manager/power_manager.h"
#include "universal_module_system/transducers/sensors/sensors_manager.h"
#include "universal_module_system/transducers/actuators/actuators_manager.h"
#include "universal_module_system/ota/ota.h"

namespace Comms {
    // ============================ Public ============================
    Connection &Connection::getInstance(
        Communication *communication,
#ifdef CENTRAL_UNIT
        const std::shared_ptr<CentralUnitAddressing> &addressing,
#else
        const std::shared_ptr<ModuleAddressing> &addressing,
#endif
        const std::shared_ptr<HC12> &rfModule,
        const std::shared_ptr<ul::Logger> &logger
    ) {
        static Connection instance(communication, addressing, rfModule, logger);
        return instance;
    }

    void Connection::messageDecider(const uint8_t receivedMessage[MESSAGE_SIZE]) {
        auto &powerManager = ums::PowerManager::getInstance(mpLogger);
        powerManager.restartIdleTimer();

        uint8_t sendBuffer[MESSAGE_SIZE] = {};
        using CT = API::commandTypes;
        using ET = API::errorTypes;
        std::optional<API::CommandHandler> receivedCommand;

        // decode command and handle decoding failure
        try {
            receivedCommand.emplace(receivedMessage);
        } catch (std::exception &e) {
            mpLogger->error("Connection messageDecider", "Failed to decode received command.");
            mpLogger->error("Connection messageDecider", e.what());
            receivedCommand.reset();
        }
        if (!receivedCommand.has_value()) {
            std::optional<API::CommandHandler> errorCommand;
            try {
                errorCommand.emplace(CT::RESPONSE);
                errorCommand->addParameter(API::APIParameter((uint8_t) ET::BAD_COMMAND, true));
            } catch (std::exception &e) {
                mpLogger->error("Connection messageDecider", "Failed to create error command.");
                mpLogger->error("Connection messageDecider", e.what());
            }

            if (errorCommand.has_value()) {
                errorCommand->generateMessage(sendBuffer);
                mpCommunication->sendMessage(sendBuffer);
            }
            return;
        }

        std::optional<API::CommandHandler> sendCommand;
        std::optional<uint32_t> uid;
        bool isDeepSleep = false;

        mpLogger->verbosev(
            "MessageDecider",
            "Number of command's parameters: ",
            static_cast<int>(receivedCommand->getNumberOfParameters())
        );

        // main decider switch
        switch (receivedCommand->getCommandType()) {
            // if DEBUG_MODE is not defined CT::RESPONSE will work same as cases: CT::ACKNOWLEDGE, CT::NEGATIVE, CT::REPING
            case CT::RESPONSE:
#ifdef DEBUG_MODE
            {
                mpLogger->verbose("MessageDecider", "CT::RESPONSE");

                try {
                    char text[16] = {};
                    std::get<API::APIParameter<char *> >(receivedCommand->getParameter(0)).getValue(text);
                    mpLogger->infoa("MessageDecider TEST", "get char array: ", reinterpret_cast<uint8_t *>(text), strlen(text));
                } catch (...) {
                }

                try {
                    constexpr size_t TEXT_SIZE = 16;
                    char text[TEXT_SIZE] = {};

                    std::array<uint8_t, ums::Ota::s_IP_ADDRESS_LENGTH> ipAddress{};
                    std::get<API::APIParameter<uint8_t *> >(receivedCommand->getParameter(1)).getValue(ipAddress.data());

                    snprintf(text, TEXT_SIZE, "%i.%i.%i.%i", ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
                    mpLogger->infoa("MessageDecider TEST", "IP: ", reinterpret_cast<uint8_t *>(text), strlen(text));
                } catch (...) {
                }

                try {
                    const auto resUid = receivedCommand->getParameterValue<uint32_t>(0);
                    mpLogger->infov("MessageDecider TEST", "uid: ", static_cast<int>(resUid));
                } catch (...) {
                }

                try {
                    const auto batteryRead = receivedCommand->getParameterValue<uint8_t>(1);
                    mpLogger->infov("MessageDecider TEST", "Battery read %: ", batteryRead);
                } catch (...) {
                }

                try {
                    const auto humidity = receivedCommand->getParameterValue<float>(1);
                    const auto pressure = receivedCommand->getParameterValue<float>(2);
                    const auto temperature = receivedCommand->getParameterValue<float>(3);
                    mpLogger->infov("MessageDecider TEST", "humidity: ",    static_cast<int>(humidity));
                    mpLogger->infov("MessageDecider TEST", "pressure: ",    static_cast<int>(pressure));
                    mpLogger->infov("MessageDecider TEST", "temperature: ", static_cast<int>(temperature));
                } catch (...) {
                    try {
                        const auto humidity = receivedCommand->getParameterValue<float>(1);
                        const auto temperature = receivedCommand->getParameterValue<float>(2);
                        mpLogger->infov("MessageDecider TEST", "humidity: ",    static_cast<int>(humidity));
                        mpLogger->infov("MessageDecider TEST", "temperature: ", static_cast<int>(temperature));
                    } catch (...) {
                    }
                }


                try {
                    sendCommand.emplace(CT::END);
                } catch (std::exception &e) {
                    sendCommand.reset();
                    mpLogger->error("MessageDecider CT::RESPONSE", e.what());
                }
                break;
            }
#endif
            case CT::ACKNOWLEDGE:
            case CT::NEGATIVE:
            case CT::REPING: {
                size_t size = snprintf(nullptr, 0, "Got %s", receivedCommand->getCommandTypeString().c_str());
                char warningMessage[size];
                sprintf(warningMessage, "Got %s", receivedCommand->getCommandTypeString().c_str());
                mpLogger->warning("Connection messageDecide", warningMessage);

                if (mConnectionTimeoutTimer != nullptr) {
                    xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
                }
                break;
            }

            case CT::REPEAT:
                mpLogger->verbose("MessageDecider", "CT::REPEAT");
                repeatLastTransmittedMessage();
                break;

            case CT::END:
                mpLogger->verbose("MessageDecider", "CT::END");
                endConnection();
                break;

            case CT::PING: {
                mpLogger->verbose("MessageDecider", "CT::PING");

                try {
                    sendCommand.emplace(CT::REPING);
                } catch (std::exception &e) {
                    sendCommand.reset();
                    mpLogger->error("MessageDecider CT::PING", e.what());
                    break;
                }

                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                    if (uid.has_value()) sendCommand->addParameter(API::APIParameter(uid.value()));
                } catch (std::exception &e) {
                    mpLogger->error("MessageDecider CT::PING", e.what());
                    responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                    break;
                }

                const size_t numberOfParameters = receivedCommand->getNumberOfParameters();
                const auto &parametersSpecialBytes = receivedCommand->getParametersSpecialBytes();

                using PT = API::parametersTypes;
                for (size_t i = 1; i < numberOfParameters; i++) {
                    try {
                        constexpr uint8_t MAX_PARAMETER_ARRAY_SIZE = 16;

                        char logMessage[32];
                        sprintf(logMessage, "Parameter type: %s", API::parametersTypesToString(receivedCommand->getParameterType(i)).data());
                        mpLogger->info("MessageDecider CT::PING", logMessage);

                        switch (receivedCommand->getParameterType(i)) {
                            case PT::INT: {
                                const auto parameterValue = receivedCommand->getParameterValue<int64_t>(i);
                                mpLogger->infov("MessageDecider CT::PING", "Parameter value: ", static_cast<int>(parameterValue));
                                sendCommand->addParameter(API::APIParameter(parameterValue));
                                break;
                            }

                            case API::parametersTypes::UINT: {
                                const auto parameterValue = receivedCommand->getParameterValue<uint64_t>(i);
                                mpLogger->infov("MessageDecider CT::PING", "Parameter value: ", static_cast<int>(parameterValue));
                                sendCommand->addParameter(API::APIParameter(parameterValue));
                                break;
                            }

                            case API::parametersTypes::FLOAT: {
                                const auto parameterValue = receivedCommand->getParameterValue<double>(i);
                                mpLogger->infov("MessageDecider CT::PING", "Parameter value: ", static_cast<int>(parameterValue));
                                sendCommand->addParameter(API::APIParameter(parameterValue));
                                break;
                            }

                            case API::parametersTypes::ASCII: {
                                char parameterValue[MAX_PARAMETER_ARRAY_SIZE] = {};
                                receivedCommand->getParameterValueArray<char>(parameterValue, i);
                                mpLogger->infoa("MessageDecider CT::PING", "Parameter value: ", reinterpret_cast<uint8_t*>(parameterValue), MAX_PARAMETER_ARRAY_SIZE);
                                sendCommand->addParameter(API::APIParameter(parameterValue, parametersSpecialBytes[i].getLength()));
                                break;
                            }

                            case API::parametersTypes::RAW: {
                                uint8_t parameterValue[MAX_PARAMETER_ARRAY_SIZE] = {};
                                receivedCommand->getParameterValueArray<uint8_t>(parameterValue, i);
                                mpLogger->infoa("MessageDecider CT::PING", "Parameter value: ", parameterValue, MAX_PARAMETER_ARRAY_SIZE, false);
                                sendCommand->addParameter(API::APIParameter(parameterValue, parametersSpecialBytes[i].getLength()));
                                break;
                            }

                            case API::parametersTypes::ERROR: {
                                const auto parameterValue = receivedCommand->getParameterValue<uint8_t>(i);
                                mpLogger->infov("MessageDecider CT::PING", "Parameter value: ", static_cast<int>(parameterValue));
                                sendCommand->addParameter(API::APIParameter(parameterValue, true));
                                break;
                            }

                            default:
                                throw std::invalid_argument("Unknown argument type.");
                        }
                    } catch (std::exception &e) {
                        mpLogger->error("MessageDecider CT::PING", e.what());
                        responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                        break;
                    }
                }
                break;
            }

            case CT::DEEP_SLEEP:
                isDeepSleep = true;
            case CT::SLEEP: {
                mpLogger->verbose("MessageDecider", "CT::DEEP_SLEEP");

                std::optional<uint32_t> sleepTime;
                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                    sleepTime = receivedCommand->getParameterValue<uint32_t>(1);
                } catch (std::exception &e) {
                    mpLogger->error("MessageDecider CT::SLEEP", "Did not get uid and/or sleepTime");
                    responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                    break;
                }

                xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
                mSleepTime = sleepTime.value();
                mIsDeepSleep = isDeepSleep;
                xSemaphoreGive(mConnectionDataMutex);

                try {
                    sendCommand.emplace(CT::RESPONSE);
                    sendCommand->addParameter(API::APIParameter(uid.value()));
                    sendCommand->addParameter(API::APIParameter(sleepTime.value()));
                } catch (std::exception &e) {
                    sendCommand.reset();
                    mpLogger->error("MessageDecider CT::DEEP_SLEEP", e.what());
                }
                break;
            }

            case CT::GET: {
                mpLogger->verbose("MessageDecider", "CT::GET");

                // get always required arguments for CT::GET
                std::optional<uint8_t> getType;
                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                    getType = receivedCommand->getParameterValue<uint8_t>(1);
                } catch (std::exception &e) {
                    responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                    break;
                }

                using GT = API::getTypes;
                auto &sensorManager = ums::Transducers::SensorsManager::getInstance(mpLogger);
                switch (getType.value()) {
                    case static_cast<uint8_t>(GT::SENSOR_VALUE_WITH_FORCE_NEW_READING):
                        mpLogger->verbose("MessageDecider CT::GET", "CT::SENSOR_VALUE_WITH_FORCE_NEW_READING");
                        sensorManager.clearCachedReadings();
                    case static_cast<uint8_t>(GT::SENSOR_VALUE): {
                        mpLogger->verbose("MessageDecider CT::GET", "CT::SENSOR_VALUE");
                        std::optional<uint8_t> sensorId;
                        try {
                            sensorId = receivedCommand->getParameterValue<uint8_t>(2);
                        } catch (std::exception &e) {
                            responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                            break;
                        }

                        try {
                            sendCommand.emplace(CT::RESPONSE);
                        } catch (std::exception &e) {
                            mpLogger->error("MessageDecider GT::BATTERY_STATE", e.what());
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }

                        try {
                            sendCommand->addParameter(API::APIParameter(uid.value()));
                            for (const auto &param: sensorManager.getSensorReading(sensorId.value()))
                                sendCommand->addParameter(param);
                        } catch (std::exception &e) {
                            sendCommand.reset();
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }
                        break;
                    }

                    case static_cast<uint8_t>(GT::CONFIG_VALUE): {
                        mpLogger->verbose("MessageDecider CT::GET", "CT::CONFIG_VALUE");

                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    case static_cast<uint8_t>(GT::SENSOR_LIST): {
                        mpLogger->verbose("MessageDecider CT::GET", "CT::SENSOR_LIST");

                        try {
                            sendCommand.emplace(CT::RESPONSE);
                        } catch (std::exception &e) {
                            mpLogger->error("MessageDecider GT::SENSOR_LIST", e.what());
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }

                        try {
                            sendCommand->addParameter(API::APIParameter(uid.value()));
                            for (const auto &param: sensorManager.getSensorsIds())
                                sendCommand->addParameter(param);
                            // CHECKME
                            for (const auto &param: ums::Transducers::ActuatorsManager::getActuatorsIds())
                                sendCommand->addParameter(param);
                        } catch (std::exception &e) {
                            sendCommand.reset();
                            mpLogger->error("MessageDecider GT::SENSOR_LIST", e.what());
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }
                        break;
                    }

                    case static_cast<uint8_t>(GT::LOGS): {
                        mpLogger->verbose("MessageDecider CT::GET", "CT::LOGS");
                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    case static_cast<uint8_t>(GT::BATTERY_STATE): {
                        constexpr uint8_t BATTERY_SENSOR_ID = 1;
                        mpLogger->verbose("MessageDecider CT::GET", "CT::BATTERY_STATE");

                        try {
                            sendCommand.emplace(CT::RESPONSE);
                        } catch (std::exception &e) {
                            sendCommand.reset();
                            mpLogger->error("MessageDecider GT::BATTERY_STATE", e.what());
                            break;
                        }

                        try {
                            sendCommand->addParameter(API::APIParameter(uid.value()));
                            for (const auto &param: sensorManager.getSensorReading(BATTERY_SENSOR_ID))
                                sendCommand->addParameter(param);
                        } catch (std::exception &e) {
                            sendCommand.reset();
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }
                        break;
                    }

                    case static_cast<uint8_t>(GT::ACTUATOR_STATE): {
                        mpLogger->verbose("MessageDecider CT::GET", "CT::ACTUATOR_STATE");
                        std::optional<uint8_t> actuatorId;
                        try {
                            actuatorId = receivedCommand->getParameterValue<uint8_t>(2);
                        } catch (std::exception &e) {
                            responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                            break;
                        }

                        try {
                            sendCommand.emplace(CT::RESPONSE);
                        } catch (std::exception &e) {
                            mpLogger->error("MessageDecider GT::ACTUATOR_STATE", e.what());
                            responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            break;
                        }
                        auto &actuatorManager = ums::Transducers::ActuatorsManager::getInstance(mpLogger);
                        sendCommand->addParameter(API::APIParameter(uid.value()));
                        sendCommand->addParameter(actuatorManager.getActuatorState(actuatorId.value()));
                        break;
                    }

                    default: {
                        mpLogger->errorv("MessageDecider CT::GET", "Got unknown get type: ", getType.value());
                        responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                        break;
                    }
                }
                break;
            }

            case CT::SET: {
                mpLogger->verbose("MessageDecider", "CT::SET");

                // get always required arguments for CT::SET
                std::optional<uint8_t> setType;

                try {
                    uid = receivedCommand->getParameterValue<uint32_t>(0);
                    setType = receivedCommand->getParameterValue<uint8_t>(1);
                } catch (std::exception &e) {
                    responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                    break;
                }

                // setup response command
                try {
                    sendCommand.emplace(CT::RESPONSE);
                } catch (std::exception &e) {
                    sendCommand.reset();
                    mpLogger->error("MessageDecider GT::BATTERY_STATE", e.what());
                    break;
                }
                sendCommand->addParameter(API::APIParameter(uid.value()));

                // execute the command and add its result
                using ST = API::setTypes;
                auto &actuatorManager = ums::Transducers::ActuatorsManager::getInstance(mpLogger);
                switch (setType.value()) {
                    case static_cast<uint8_t>(ST::CHANGE_CONFIG): {
                        mpLogger->verbose("MessageDecider CT::SET", "ST::CHANGE_CONFIG");
                        responseWithError(sendCommand, ET::NOT_IMPLEMENTED, uid);
                        break;
                    }

                    case static_cast<uint8_t>(ST::ACTUATOR_OPERATION): {
                        std::optional<uint8_t> actuatorId;
                        std::optional<API::APIParameterVariant> operation;
                        // WARNING: to actuatorDoOperation is passed raw API::APIParameterVariant
                        try {
                            actuatorId = receivedCommand->getParameterValue<uint8_t>(2);
                            operation = receivedCommand->getParameter(3);
                        } catch (std::exception &e) {
                            responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                            break;
                        }

                        sendCommand->addParameter(actuatorManager.actuatorDoOperation(
                            actuatorId.value(),
                            operation.value()
                        ));
                        break;
                    }

                    case static_cast<uint8_t>(ST::ACTUATOR_TOGGLE): {
                        std::optional<uint8_t> actuatorId;
                        try {
                            actuatorId = receivedCommand->getParameterValue<uint8_t>(2);
                        } catch (std::exception &e) {
                            responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                            break;
                        }

                        sendCommand->addParameter(actuatorManager.toggleActuator(actuatorId.value()));
                        break;
                    }

                    case static_cast<uint8_t>(ST::OTA): {
                        using OTAO = ums::otaOperations;

                        mpLogger->verbose("MessageDecider", "CT::OTA");
                        std::optional<uint8_t> operation;
                        try {
                            operation = receivedCommand->getParameterValue<uint8_t>(2);
                        } catch (std::exception &e) {
                            responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                            break;
                        }

                        auto &ota = ums::Ota::getInstance();
                        if (operation.value() == static_cast<uint8_t>(OTAO::END)) {
                            ota.endOta();
                            sendCommand->addParameter(API::APIParameter(static_cast<uint8_t>(OTAO::END)));
                        }
                        // on
                        else {
                            const std::array<uint8_t, ums::Ota::s_IP_ADDRESS_LENGTH> ipAddress = ota.beginOta();
                            if (ota.isConnectedToWifi()) {
                                sendCommand->addParameter(
                                    API::APIParameter<uint8_t *>(ipAddress.data(), ums::Ota::s_IP_ADDRESS_LENGTH)
                                );
                            } else {
                                responseWithError(sendCommand, ET::INTERNAL_ERROR, uid);
                            }
                        }
                        break;
                    }

                    default: {
                        mpLogger->errorv("MessageDecider CT::SET", "Got unknown set type: ", setType.value());
                        responseWithError(sendCommand, ET::BAD_ARGUMENT, uid);
                        break;
                    }
                }
                break;
            }

            case CT::NOTIFY: {
                mpLogger->verbose("MessageDecider", "CT::NOTIFY");
                try {
                    sendCommand.emplace(CT::ACKNOWLEDGE);
                } catch (std::exception &e) {
                    receivedCommand.reset();
                    mpLogger->error("MessageDecider CT::NOTIFY", e.what());
                }
#ifdef DEBUG_MODE
                try {
                    auto notifType = receivedCommand->getParameterValue<uint8_t>(0);
                    mpLogger->verbosev("MessageDecider", "CT::NOTIFY type: ", notifType);
                } catch (...){}
#endif
                break;
            }

            default:
                mpLogger->errorv(
                    "Connection messageDecider",
                    "Received unknown command type: ",
                    (int) receivedCommand->getCommandType()
                );
                responseWithError(sendCommand, ET::UNKNOWN_COMMAND, uid);
                break;
        }

        // send response if successfully created command
        if (sendCommand.has_value()) {
            sendCommand->generateMessage(sendBuffer);
            mpCommunication->sendMessage(sendBuffer);

        // TODO !mm uncomment #ifdef CENTRAL_UNIT
        // #ifdef CENTRAL_UNIT
            if (sendCommand->getCommandType() == CT::END)
                endConnection();
        // #endif
        }
    }

    void Connection::responseWithError(std::optional<API::CommandHandler> &responseCommand, API::errorTypes errorType,
                                       const std::optional<uint32_t> uid) const {
        using CT = API::commandTypes;
        if (!uid.has_value()) {
            try {
                responseCommand.emplace(CT::RESPONSE);
                responseCommand->addParameter(API::APIParameter((uint8_t) errorType, true));
            } catch (std::exception &e) {
                responseCommand.reset();
                mpLogger->error("Connection messageDecider", e.what());
            }
            return;
        }

        try {
            responseCommand.emplace(CT::RESPONSE);
            responseCommand->addParameter(API::APIParameter(uid.value()));
            responseCommand->addParameter(API::APIParameter((uint8_t) errorType, true));
        } catch (std::exception &e) {
            responseCommand.reset();
            mpLogger->error("Connection messageDecider", e.what());
        }
    }

    void Connection::receivingHandle(const uint8_t ip) {
        if (mpAddressing->getIsAddressingInProgress()) return;

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        createConnectionTimer();
        xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpAddressing->setProtocolIPAddress(ip); // do anything only for Central Unit
        }
        // if received any message while rf was changed, save rf channel
        if (mTmpChannel.has_value()) {
            mpAddressing->changeRfChannel(mTmpChannel.value());
            mTmpChannel.reset();
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::sendingHandle(const uint8_t message[MESSAGE_SIZE]) {
        auto &powerManager = ums::PowerManager::getInstance(mpLogger);
        powerManager.restartIdleTimer();

        if (
            const API::CommandHandler ch(message);
            ch.getCommandType() != API::commandTypes::REPEAT
        ) {
            mpLogger->debug("Connection Method", "setLastTransmittedMessage(message)");
            setLastTransmittedMessage(message);
        }
        if (mpAddressing->getIsAddressingInProgress()) return;

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (!mIsConnected) {
            mpLogger->debug("Connection Class", "Connection start.");
            mIsConnected = true;
            mpRfModule->firstChangeRFChannel(mpAddressing->getConnectionRFChannel());

            createConnectionTimer();
            xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::endConnection() {
        mpLogger->debug("Connection Class", "Connection end.");
        if (mConnectionTimeoutTimer != nullptr) {
            xTimerStop(mConnectionTimeoutTimer, portMAX_DELAY);
            deleteConnectionTimer();
        }

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        mIsConnected = false;
        uah::clearBuffer(mConnectionFailedData.lastMessage, MESSAGE_SIZE);
        mConnectionFailedData.attempts = 0;
        xSemaphoreGive(mConnectionDataMutex);

        mpAddressing->setProtocolIPAddress(NULL_IP); // do anything only for Central Unit
        afterConnectionEndHandler();
    }

    void Connection::transmitRepeatMessage() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mIsConnected || mpAddressing->getIsAddressingInProgress()) {
            std::optional<API::CommandHandler> responseCommand;
            try {
                responseCommand.emplace(API::commandTypes::REPEAT);
            } catch (std::exception &e) {
                mpLogger->error("Connection transmitRepeatMessage", "Failed to create REPEAT command.");
                mpLogger->error("Connection messageDecider", e.what());
            }

            if (responseCommand.has_value()) {
                uint8_t buffer[MESSAGE_SIZE] = {};
                responseCommand->generateMessage(buffer);
                mpCommunication->sendPriorityMessage(buffer);
            }
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    // ================== Constructor and Destructor ==================
    Connection::Connection(
        Communication *communication,
#ifdef CENTRAL_UNIT
        const std::shared_ptr<CentralUnitAddressing> &addressing,
#else
        const std::shared_ptr<ModuleAddressing> &addressing,
#endif
        const std::shared_ptr<HC12> &rfModule,
        const std::shared_ptr<ul::Logger> &logger
    ) : mpCommunication(communication), mpAddressing(addressing), mpRfModule(rfModule), mpLogger(logger) {
        mConnectionDataMutex = xSemaphoreCreateMutex();

        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        uah::clearBuffer(mConnectionFailedData.lastMessage, MESSAGE_SIZE);
        mConnectionFailedData.attempts = 0;
        xSemaphoreGive(mConnectionDataMutex);

        mpLogger->verbose("Connection Class", "Connection initialized.");
    }

    Connection::~Connection() {
        deleteConnectionTimer();
        vSemaphoreDelete(mConnectionDataMutex);
    }

    // ============================ Timers ============================
    void Connection::connectionTimerCallback(TimerHandle_t xTimer) {
        auto &con = *static_cast<Connection *>(pvTimerGetTimerID(xTimer));
        con.mpLogger->warning("Connection Class", "Connection timeout.");

        // if timeout occurred while changing channel
        xSemaphoreTake(con.mConnectionDataMutex, portMAX_DELAY);
        if (con.mTmpChannel.has_value()) {
            con.mTmpChannel.reset();
            xSemaphoreGive(con.mConnectionDataMutex);
            con.endConnection();
        } else {
            xSemaphoreGive(con.mConnectionDataMutex);
            con.repeatLastTransmittedMessage();
        }
    }

    void Connection::createConnectionTimer() {
        if (mConnectionTimeoutTimer == nullptr) {
            mConnectionTimeoutTimer = xTimerCreate(
                "Connection Timeout",
                pdMS_TO_TICKS(mConnectionFailedData.getTimeout()),
                pdFALSE,
                this,
                connectionTimerCallback
            );
        }
    }

    void Connection::deleteConnectionTimer() {
        if (mConnectionTimeoutTimer != nullptr) {
            xTimerDelete(mConnectionTimeoutTimer, portMAX_DELAY);
            mConnectionTimeoutTimer = nullptr;
        }
    }

    // ============================ Other =============================
    uint8_t Connection::getLastTransmittedMessageAttempts() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        const uint8_t result = mConnectionFailedData.attempts;
        xSemaphoreGive(mConnectionDataMutex);
        return result;
    }

    void Connection::setLastTransmittedMessage(const uint8_t message[MESSAGE_SIZE]) {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        uah::prepareBuffer(mConnectionFailedData.lastMessage, message, MESSAGE_SIZE, MESSAGE_SIZE);
        xSemaphoreGive(mConnectionDataMutex);
    }

    void Connection::repeatLastTransmittedMessage() {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        if (mConnectionFailedData.lastMessage[0] != 0) {
            mpCommunication->sendPriorityMessage(mConnectionFailedData.lastMessage);
        }

        if (mConnectionFailedData.attempts > REPEAT_LAST_MESSAGE_MAX_ATTEMPTS) {
            xSemaphoreGive(mConnectionDataMutex);
            endConnection();
        } else {
            xTimerChangePeriod(mConnectionTimeoutTimer, mConnectionFailedData.getTimeout(), portMAX_DELAY);
            xTimerStart(mConnectionTimeoutTimer, portMAX_DELAY);
            xSemaphoreGive(mConnectionDataMutex);
        }

        mConnectionFailedData.attempts++;
    }

    void Connection::afterConnectionEndHandler() const {
        xSemaphoreTake(mConnectionDataMutex, portMAX_DELAY);
        // changing rf channel
#ifdef RF_CHANNELS
        mpRfModule->firstChangeRFChannel(mpAddressing->getDefaultRFChannel());
#else
#error "Not implemented"
#endif
        // going to sleep
        if (mSleepTime != 0) {
            auto &powerManager = ums::PowerManager::getInstance(mpLogger);
            powerManager.enterSleep(mSleepTime, !mIsDeepSleep);
        }
        xSemaphoreGive(mConnectionDataMutex);
    }

    uint16_t Connection::ConnectionFailedData::getTimeout() {
        const int32_t range = s_TIMEOUTS[attempts] * OFFSET_PERCENTAGE / 100;
        const int32_t offset = std::uniform_int_distribution<>(range * -1, range)(mGenerator);
        const uint16_t result = s_TIMEOUTS[attempts] + offset;
        return result;
    }
}
