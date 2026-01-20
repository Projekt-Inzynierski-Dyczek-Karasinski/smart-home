#error "Documentation only."

/**
 * @file namespaces.h
 * @brief Smart Home system namespace documentation.
 *
 * This file contains documentation for the namespace structure used
 * throughout the project. It is not intended for compilation.
 */

/**
 * @namespace SmartHome
 * @brief Main namespace for the smart home central unit system.
 *
 * @details Contains all components of the Smart Home Central Unit and its subsystems.
 */
namespace SmartHome {
}

/**
 * @namespace SmartHome::JsonRpcStrings
 * @brief String constants for JSON-RPC protocol implementation.
 *
 * @details Contains compile-time string constants organized by purpose:
 * - Protocol version and common values
 * - JSON-RPC standard field names
 * - Custom SmartHome-specific parameter keys
 *
 * @note All constants are defined as constexpr string_view for zero-overhead access.
 */
namespace SmartHome::JsonRpcStrings {
}

/**
 * @namespace SmartHome::API
 * @brief API layer for internal and external communication.
 *
 * @details Implements JSON-RPC 2.0 based API protocol for communication between
 * SmartHome components and external interfaces:
 * - JSON-RPC request/response structures
 * - Internal API for component-to-component communication
 * - Command routing and execution framework
 * - Error handling with standard JSON-RPC error codes
 * - Support for both structured (JSON) and raw command formats
 */
namespace SmartHome::API {
}

/**
 * @namespace SmartHome::IPC
 * @brief Inter-Process Communication layer.
 *
 * @details Implements communication layer between system components:
 * - Socket server handling TCP and Unix Domain Sockets
 * - Client connection management
 * - Internal communication protocols
 *
 * @note This namespace implements only transport, not application protocol.
 */
namespace SmartHome::IPC {
}

/**
 * @namespace SmartHome::Utils
 * @brief Utility tools and common components.
 *
 * @details Contains shared utilities used by various modules:
 * - ConfigManager - YAML configuration management
 * - ServiceManager - Contains virtual components responsible for operating system integration:
 *      - Systemd integration (Linux)
 *      - Standalone mode with multiple instance prevention
 *      - Abstractions for different service managers
 * - Logger - Console and file logging.
 * - FileLock - RAII wrapper for exclusive file locking
 * - Helper functions for conversion and validation
 */
namespace SmartHome::Utils {
}

/**
 * @namespace SmartHome::Exceptions
 * @brief Custom exceptions for Smart Home.
 *
 * @details Implements custom exception types based on \c std::exception class. \n Used in:
 * - SmartHome::Utils
 */
namespace SmartHome::Exceptions {
}

/**
 * @namespace SmartHomeCLI
 * @brief Command Line Interface for Smart Home system.
 *
 * @details Separate namespace for CLI application that:
 * - Is an independent program communicating with Core via IPC
 * - Uses SmartHome::IPC components for connection
 *
 * @note This is a separate program, not part of Core.
 *
 */
namespace SmartHomeCLI {
}

/**
 * @namespace SmartHomeGUI
 * @brief Graphic User Interface for Smart Home system.
 *
 * @details Separate namespace for GUI application that:
 * - Is an independent program communicating with Core via UDS IPC using internal API
 * - Uses SmartHome::Logger, SmartHome::Utils
 *
 * @note This is a separate program, not part of Core.
 */
namespace SmartHomeGUI {
}

/**
 * @namespace SmartHomeMediator
 * @brief RF communication module for managing wireless smart home devices.
 *
 * @details Implements RF (Radio Frequency) communication layer for Smart Home modules:
 * - RF driver management (HC-12 transceiver support)
 * - UART serial port communication with async operations
 * - Session management for RF command/response handling
 * - Protocol translation between Internal API and RF protocol
 * - Configuration mode support for RF module setup
 * - Asynchronous read/write operations with timeout handling
 *
 * Architecture:
 * - Acts as a bridge between Core (via IPC) and RF modules (via UART)
 * - Maintains active sessions for request-response correlation
 * - Supports both synchronous config commands and async module communication
 *
 * @note This is a separate program that communicates with Core via TCP/UDS.
 */
namespace SmartHomeMediator {
}

/**
 * @namespace SmartHomeMediator::RfTypes
 * @brief RF protocol data structures and type definitions.
 *
 * @details Defines RF communication protocol types and structures:
 * - Command types: GET, SET, EXECUTE, NOTIFY, PING, SLEEP, DEEP_SLEEP
 * - MediatorConfigCommand - configuration commands for RF module itself
 * - RfCommand - commands forwarded to wireless modules
 * - Parameter serialization/deserialization for RF protocol
 * - Request type enums (GetType, SetType, NotificationType, etc.)
 * - Protocol constants and string mappings
 *
 * Key concepts:
 * - Config commands control the mediator (baudrate, channel, power)
 * - RF commands are forwarded to wireless modules via RF
 * - Requests have IDs for correlation, notifications do not
 *
 * @note Protocol is optimized for low-bandwidth RF communication.
 */
namespace SmartHomeMediator::RfTypes {
}
