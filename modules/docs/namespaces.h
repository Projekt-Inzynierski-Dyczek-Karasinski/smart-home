#error "Documentation only."

/**
 * @namespace Comms
 * @brief Communication management for internal and RF communication.
 * @details Handles all communication in the project, both internal (between different components of project) and RF communication. Contains:
 * - class Communication - class maintaining and handling internal and RF communication
 * - class HC12 - driver for HC12 module used for RF communication
 * - abstract class Addressing - class storing and handling data about used RF channel, "IP" and MAC addresses.
 * - derived class CentralUnitAddressing - Addressing class with central unit's features
 * - derived class ModuleAddressing - Addressing class with module's features
 * - class Connection - handling communication after addressing, when module is exchanging messages with central unit
 * - namespace API - containing classes that handles communication API
 */
namespace Comms {}

/**
 * @namespace Comms::API
 * @brief Contains classes that handle communication API.
 * @details Handles API for RF communication. Introduces features that enable easy encoding and decoding commands
 * and parameters. Contains:
 * - class CommandHandler - main API class, that enables converting raw message's bytes to command and vice versa.
 * - class SpecialByteCommand - handles API special byte for commands.
 * - class SpecialByteParameter - handles API special byte for command's parameters.
 * - class APIParameter - handles creating and decoding parameters passed in commands.
 * - multiple enum classes - for commands, errors, parameters, etc. types.
 */
namespace Comms::API {}

/**
 * @namespace UniversalModuleSystem
 * @brief Features common for all modules in SMART Home system.
 *
 * @details Contains:
 * - class DebugLED - class handling led
 * - class PairingButton - class handling button
 * - class DataManager - class handling loading and saving data from/to flash memory
 * - class PowerManager - class responsible for rebooting, putting to sleep and reading battery
 * - class Ota - class responsible for connecting to Wi-Fi and handling OTA programming
 * - namespace Transducers - namespace for actuators and sensors used in SMART Home system.
 *
 * @warning Class PowerManager works only with ESP32.
 */
namespace UniversalModuleSystem {}

/**
 * @namespace UniversalModuleSystem::Transducers
 * @brief Contains classes for handling actuators and sensors used in SMART Home system.
 *
 * @details Contains:
 * - class SensorManager - singleton handling all module's sensors
 * - class Sensor - abstract Sensor base class
 * - class ActuatorManager - singleton handling all module's actuators
 * - class Actuator - abstract Actuator base class
 * - multiple derived Sensor classes
 * - multiple derived Actuator classes
 */
namespace UniversalModuleSystem::Transducers {}

/**
 * @namespace Utils
 * @brief Utility tools and common components.
 *
 * @details Contains shared utilities used in entire project:
 * - Logging - namespace containing Logger class and enum Level
 * - ArrayHandlers - namespace containing utility functions for arrays
 */
namespace Utils {}

/**
 * @namespace Utils::Logging
 * @brief Namespace containing methods used for logging.
 * @details Contains:
 * - enum Level
 * - class Logger
 */
namespace Utils::Logging {}

/**
 * @namespace Utils::ArrayHandlers
 * @brief Namespace containing utility functions for arrays.
 * @details Available functions:
 * - void printArrayAsChar()
 * - void printArrayAsInt()
 * - void prepareBuffer()
 * - uint8_t calcLenOfDataInArray()
 * - bool areArraysEqual()
 */
namespace Utils::ArrayHandlers {}

