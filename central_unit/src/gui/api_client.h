#pragma once

#include <QObject>
#include <QStack>
#include <QLocalSocket>

using namespace std::chrono_literals;

namespace SmartHomeGUI {
    class ApiClient: public QObject {
        Q_OBJECT
    public:
        explicit ApiClient(QObject *parent = nullptr);

        ~ApiClient() override;

        /**
         * @brief Attempts establishing connection with SmartHome daemon IPC server.
         *
         * @param udsPath Path to SmartHome daemon IPC Unix Domain Socket.
         *
         * @return true if connected successfully, false otherwise.
         */
        bool connectToServer(std::string_view udsPath);

        /**
         * @brief Send JSON-RPC 2.0 request to SmartHome daemon and return response.
         *
         * @param request JSON-RPC 2.0 request message to send to SmartHome daemon.
         *
         * @return QString containing JSON-RPC 2.0 response, empty Qstring on failed fetch.
         */
        Q_INVOKABLE QString sendRequest(const QString &request) ;

        /**
         * @brief Send JSON-RPC 2.0 notification to SmartHome daemon.
         *
         * @param notification JSON-RPC 2.0 notification message to send to SmartHome daemon.
         */
        Q_INVOKABLE void sendNotification(const QString &notification) ;

    private:
        QLocalSocket mSocket;

        static constexpr std::string_view messageDelimiter = "\r\n";
        static constexpr auto ms_CONNECTION_TIMEOUT = 1000ms;
        static constexpr auto ms_REQUEST_TIMEOUT = 5000ms;
    };
}
