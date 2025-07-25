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