#pragma once

#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

#include "universal_module_system/transducers/i_transducer.h"
#include "utils/logger.h"

namespace nl = nlohmann;
namespace ul = Utils::Logging;

namespace UniversalModuleSystem::Transducers {
    class Sensor : public ITransducer {
    public:
        // TODO consider changing String to char
        Sensor(const std::shared_ptr<ul::Logger> &logger, String dataPath);

        ~Sensor() override;

        //virtual uint32_t getRawRead() = 0; // TODO !pr consider removing
        //virtual uint32_t getFormatedRead() = 0; // TODO !pr consider removing

        virtual String getApiFormatedRead() = 0;

        virtual void calibrate() = 0;

        uint8_t getId() override;
        void onBoot() override;

    protected:
        virtual void read() = 0;

        virtual void loadAdditionalData(const nl::json& json) = 0;

        void loadData() override;


        struct SensorData {
            explicit SensorData(const nl::json& json);

            // TODO !pr remove if not needed
            // nl::json toJson();

            uint8_t id;
            uint8_t readPin; // TODO consider making this array
            std::optional<uint8_t> powerPin;
            bool canAwake;
        private:
            static constexpr char ms_ID[] = "id";
            static constexpr char ms_READ_PIN[] = "readPin";
            static constexpr char ms_POWER_PIN[] = "powerPin";
            static constexpr char ms_CAN_AWAKE[] = "canAwake";
        };

        std::shared_ptr<ul::Logger> mpLogger;
        std::optional<SensorData> mSensorData;
        const String mDataPath;

        SemaphoreHandle_t mSensorDataMutex;
    };
}