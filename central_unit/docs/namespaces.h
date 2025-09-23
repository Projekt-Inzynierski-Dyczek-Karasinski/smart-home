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
 * @namespace SmartHome::Service
 * @brief System service management and application lifecycle.
 *
 * @details Contains components responsible for operating system integration:
 * - Systemd integration (Linux)
 * - Standalone mode with multiple instance prevention
 * - Abstractions for different service managers
 */
namespace SmartHome::Service {
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
 * - FileLock - RAII wrapper for exclusive file locking
 * - Helper functions for conversion and validation
 */
namespace SmartHome::Utils {
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
