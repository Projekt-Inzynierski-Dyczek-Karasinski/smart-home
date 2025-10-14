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
 */
namespace Comms {}

/**
 * @namespace UniversalModuleSystem
 * @brief Features common for all modules in SMART Home system.
 *
 * @details Contains:
 * - class DebugLED - class handling led
 * - class PairingButton - class handling button
 * - class DataManager - class handling loading and saving data from/to flash memory
 * - class PowerManager - class responsible for rebooting, putting to sleep and reading battery
 *
 * @warning Class PowerManager works only with ESP32.
 */
namespace UniversalModuleSystem {}

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