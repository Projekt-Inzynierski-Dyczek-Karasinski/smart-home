#include "api_client.h"

namespace SmartHomeGUI {
    ApiClient::ApiClient(QObject *parent) : QObject(parent) {
    }

    ApiClient::~ApiClient() {
        if (mSocket.open()) { mSocket.disconnectFromServer(); }
    }

    bool ApiClient::connectToServer(const std::string_view udsPath) {
        mSocket.connectToServer(udsPath.data());

        // TODO implement handshake via "core set connection_type: gui" request,
        //  allowing core to send alerts and notifications to gui

        if (!mSocket.waitForConnected(ms_CONNECTION_TIMEOUT.count())) {
            return false;
        }
        return true;
    }

    QString ApiClient::sendRequest(const QString &request) {
        mSocket.write(request.toUtf8() + ms_MESSAGE_DELIMITER.data());
        mSocket.flush();

        if (!mSocket.waitForReadyRead(ms_REQUEST_TIMEOUT.count())) {
            return "";
        }

        QString response = QString::fromUtf8(mSocket.readAll());
        mSocket.flush();
        return response;
    }

    void ApiClient::sendNotification(const QString &notification) {
        mSocket.write(notification.toUtf8() + ms_MESSAGE_DELIMITER.data());
        mSocket.flush();
    }
}
