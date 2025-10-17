#include "main.h"

#include "api_client.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>


namespace SmartHomeGUI {
    void loadLoggerYamlConfig(su::ConfigManager &configManager, su::Logger::Config &config) {
        std::string root = "gui.logging";
        config.logLevel = su::LogLevels::toLevel(
            configManager.getValue<int>(root + ".log_level").
            value_or(static_cast<int>(su::LogLevels::defaultLevel)));
        configManager.getValue(root + ".enable_console_log_output", config.enableConsoleLogOutput);

        root += ".log_file";
        configManager.getValue(root + ".enabled", config.logFile.enabled);
        configManager.getValue(root + ".create_new", config.logFile.createNew);
        configManager.getValue(root + ".archive_old", config.logFile.archiveOld);
        configManager.getValue(root + ".path", config.logFile.path);
    }

    void loadGuiYamlConfig(su::ConfigManager &configManager, GuiConfig &config) {
        std::string root = "core.ipc.uds";

        configManager.getValue(root + ".enabled", config.udsEnabled);
        configManager.getValue(root + ".socket_path", config.udsPath);
    }

    void loadConfigs(const std::shared_ptr<su::Logger> &logger, GuiConfig &config) {
        auto configManager = su::ConfigManager(logger);
        su::Logger::Config loggerConfig;

        if (configManager.loadConfig(s_DEFAULT_CONFIG_PATH.data())) {
            logger->debug("[MAIN] Loading YAML logger config");
            loadLoggerYamlConfig(configManager, loggerConfig);

            logger->debug("[MAIN] Loading YAML GUI config");
            loadGuiYamlConfig(configManager, config);
        } else {
            logger->error("[MAIN] Could not load YAML config");
        }

        logger->applyConfig(loggerConfig);
    }
}


int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "wayland;xcb");
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // TODO consider adding program options beside Qt options
    auto logger = std::make_shared<su::Logger>();
    SmartHomeGUI::GuiConfig guiConfig;

    SmartHomeGUI::loadConfigs(logger, guiConfig);

    if (!guiConfig.udsEnabled) {
        logger->critical("[MAIN] UDS disabled in smarthome daemon config");
        return EXIT_FAILURE;
    }

    SmartHomeGUI::ApiClient apiClient;
    if (!apiClient.connectToServer(guiConfig.udsPath)) {
        // TODO Consider displaying error page instead of exiting with critical
        logger->critical("[MAIN] Cannot connect to smarthome daemon");
        return EXIT_FAILURE;
    }

    engine.rootContext()->setContextProperty("apiClient", &apiClient);

    const QUrl url(u"qrc:/Smarthome/qml/Main.qml"_qs);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.load(url);

    logger->info("[MAIN] Launching Qt app");
    return app.exec();
}
