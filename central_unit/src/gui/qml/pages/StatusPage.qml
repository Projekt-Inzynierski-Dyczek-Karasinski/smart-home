import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: statusPage

    // TODO Change when implementing real fetches
    // Properties for fetched data
    property int uptime: 0
    property var services: []
    property var logs: []
    property bool apiServerConnected: true

    Timer {
        interval: 5000
        running: statusPage.visible
        repeat: true
        onTriggered: fetchStatus()
    }

    Component.onCompleted: {
        fetchStatus()
    }

    onVisibleChanged: {
        if (visible) {
            fetchStatus()
        }
    }

    function fetchStatus() {
        // TODO add real fetches

        // TODO remove mock data
        uptime = 12345
        services = [
            {name: "Database Service", status: "active"},
            {name: "Module Mediator", status: "active"},
            {name: "API Server", status: apiServerConnected ? "active" : "inactive"}
        ]
        logs = [
            "2025-10-11 14:30:22    [INFO] [Core] System started successfully",
            "2025-10-11 14:30:25    [INFO] [Core] All modules loaded",
            "2025-10-11 14:32:10    [WARN] [Module] Module X reconnecting",
            "2025-10-11 14:35:01    [INFO] [Module] Connection restored",
            "2025-10-11 14:35:15    [ERROR] [API] Test error message",
            "2025-10-11 14:35:30    [INFO] [API] Error recovered",
            "2025-10-11 14:36:00    [INFO] [Core] Heartbeat received"
        ]
    }

    function formatUptime(seconds) {
        // TODO rework if time format changes in real fetch
        const days = Math.floor(seconds / 86400)
        const hours = Math.floor((seconds % 86400) / 3600)
        const minutes = Math.floor((seconds % 3600) / 60)
        const secs = seconds % 60

        let parts = []
        if (days > 0) parts.push(days + "d")
        if (hours > 0) parts.push(hours + "h")
        if (minutes > 0) parts.push(minutes + "m")
        if (secs > 0 || parts.length === 0) parts.push(secs + "s")

        return parts.join(" ")
    }

    function getLogColor(logLine) {
        if (logLine.indexOf("[CRITICAL]") !== -1) return "#ff2600"
        if (logLine.indexOf("[ERROR]") !== -1) return "#c21d0f"
        if (logLine.indexOf("[WARNING]") !== -1) return "#f39c12"
        if (logLine.indexOf("[INFO]") !== -1) return "#3498db"
        return "#95a5a6"
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Item {
            width: statusPage.width
            implicitHeight: mainLayout.implicitHeight

            ColumnLayout {
                id: mainLayout
                anchors.fill: parent
                anchors.topMargin: 16
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                spacing: 24

                // Connection Status Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: connectionLayout.implicitHeight + 40
                    color: "#2c3e50"
                    radius: 12

                    RowLayout {
                        id: connectionLayout
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 15

                        Rectangle {
                            width: root.scaled(20)
                            height: root.scaled(20)
                            radius: root.scaled(10)
                            color: apiServerConnected ? "#2ecc71" : "#e74c3c"

                            SequentialAnimation on opacity {
                                running: apiServerConnected
                                loops: Animation.Infinite
                                NumberAnimation {
                                    to: 0.3; duration: 1000
                                }
                                NumberAnimation {
                                    to: 1.0; duration: 1000
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true

                            Text {
                                text: apiServerConnected ? "Connected to Core" : "Disconnected"
                                font.pixelSize: root.scaled(20)
                                font.bold: true
                                color: "white"
                            }

                            Text {
                                text: "Uptime: " + formatUptime(uptime)
                                font.pixelSize: root.scaled(14)
                                color: "#95a5a6"
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Refresh"
                            Layout.preferredWidth: root.scaled(100)
                            Layout.preferredHeight: root.scaled(40)

                            background: Rectangle {
                                color: parent.pressed ? "#27ae60" : "#2ecc71"
                                radius: 6
                            }

                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                font.pixelSize: root.scaled(14)
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: fetchStatus()
                            }
                        }
                    }
                }

                // Services Section
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: "Services"
                        font.pixelSize: root.scaled(24)
                        font.bold: true
                        color: "#2c3e50"
                    }

                    Repeater {
                        model: services

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: serviceLayout.implicitHeight + 24
                            color: "#ecf0f1"
                            radius: 8

                            RowLayout {
                                id: serviceLayout
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                Rectangle {
                                    width: root.scaled(12)
                                    height: root.scaled(12)
                                    radius: root.scaled(6)
                                    color: modelData.status === "active" ? "#2ecc71" : "#e74c3c"
                                }

                                Text {
                                    text: modelData.name
                                    font.pixelSize: root.scaled(16)
                                    color: "#2c3e50"
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.status === "active" ? "Active" : "Inactive"
                                    font.pixelSize: root.scaled(16)
                                    font.bold: true
                                    color: modelData.status === "active" ? "#2ecc71" : "#e74c3c"
                                }
                            }
                        }
                    }
                }

                // Logs Section
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.rightMargin: 16

                        Text {
                            text: "Recent Logs"
                            font.pixelSize: root.scaled(24)
                            font.bold: true
                            color: "#2c3e50"
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Last 10"
                            font.pixelSize: root.scaled(10)
                            color: "#7f8c8d"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: logsColumn.implicitHeight + 20
                        color: "#2c3e50"
                        radius: 8

                        ColumnLayout {
                            id: logsColumn
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Repeater {
                                model: logs

                                Text {
                                    text: modelData
                                    font.pixelSize: root.scaled(12)
                                    font.family: "monospace"
                                    color: getLogColor(modelData)
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                }
                            }

                            Text {
                                text: "No logs available"
                                font.pixelSize: root.scaled(14)
                                color: "#7f8c8d"
                                font.italic: true
                                visible: logs.length === 0
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                }
            }
        }
    }
}