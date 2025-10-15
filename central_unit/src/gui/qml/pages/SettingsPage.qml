import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../components" as Components

Item {
    id: settingsPage

    // TODO change mock data after implementing working configuration
    // Network properties
    property string currentSSID: ""
    property string ipAddress: ""
    property string macAddress: ""
    property string hostname: ""

    // Display properties
    property int brightness: 80
    property int screensaverTimeout: 5

    // Advanced properties
    property bool sshEnabled: false
    property string yamlConfig: ""

    Component.onCompleted: {
        fetchSettings()
    }

    onVisibleChanged: {
        if (visible) {
            fetchSettings()
        }
    }

    function fetchSettings() {
        // TODO Implement fetching real settings

        // TODO remove mock data after implementing real fetches
        currentSSID = "HomeNetwork"
        ipAddress = "192.168.1.100"
        macAddress = "AA:BB:CC:DD:EE:FF"
        hostname = "smarthome-device"

        brightness = 80
        screensaverTimeout = 5

        sshEnabled = false
        yamlConfig = "# Configuration\ncore:\n  port: 43321\n  modules:\n    - database\n    - api"
    }

    // Network actions
    function connectToWifi(ssid, password) {
        // TODO Implement WLAN connection
        console.log("Connecting to:", ssid)
    }

    function disconnectWifi() {
        // TODO Implement WLAN disconnect
        console.log("Disconnecting WiFi")
    }

    function setHostname(newHostname) {
        // TODO Implement set hostname
        console.log("Setting hostname to:", newHostname)
        hostname = newHostname
    }

    // Display actions
    function setBrightness(value) {
        // TODO Implement set screen brightness
        console.log("Setting brightness to:", value)
        brightness = value
    }

    function setScreensaverTimeout(minutes) {
        // TODO Implement set screensaver timeout
        console.log("Setting screensaver timeout to:", minutes, "minutes")
        screensaverTimeout = minutes
    }

    // System actions
    function reloadConfig() {
        // TODO Implement reload configuration
        console.log("Reloading configuration...")
    }

    function restartDevice() {
        // TODO Implement restart device
        console.log("Restarting device...")
    }

    function checkAndInstallUpdates() {
        // TODO Implement check and install updates
        console.log("Installing update...")
    }

    // Advanced actions
    function toggleSSH(enabled) {
        // TODO Implement enable/disable SSH
        console.log("SSH:", enabled)
        sshEnabled = enabled
    }

    function saveYamlConfig(config) {
        // TODO Implement save YAML config
        console.log("Saving YAML config...")
        yamlConfig = config
    }

    function factoryReset() {
        // TODO Implement factory reset
        console.log("Factory reset")
    }

    // Change hostname dialog
    Dialog {
        id: hostnameDialog
        title: "Change hostname"
        modal: true
        anchors.centerIn: parent

        ColumnLayout {
            spacing: 15

            TextField {
                id: hostnameField
                placeholderText: "Enter new hostname"
                text: hostname
                Layout.preferredWidth: root.scaled(300)
                Layout.preferredHeight: root.scaled(50)
                font.pixelSize: root.scaled(16)
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignRight

                Components.DialogButton {
                    label: "Cancel"
                    onClickAction: hostnameDialog.close()
                }

                Components.DialogButton {
                    label: "Save"
                    backgroundColor: "#2ecc71"
                    backgroundColorPressed: "#27ae60"
                    onClickAction: {
                        setHostname(hostnameField.text)
                        hostnameDialog.close()
                    }
                }
            }
        }
    }

    // YAML Editor dialog
    Dialog {
        id: yamlEditorDialog
        title: "Edit YAML Configuration"
        modal: true
        anchors.centerIn: parent
        width: root.width * 0.8
        height: root.height * 0.8

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: yamlEditor
                    text: yamlConfig
                    font.family: "monospace"
                    font.pixelSize: root.scaled(14)
                    wrapMode: TextArea.NoWrap
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Components.DialogButton {
                    label: "Cancel"
                    onClickAction: {
                        yamlEditor.text = yamlConfig
                        yamlEditorDialog.close()
                    }
                }

                Components.DialogButton {
                    label: "Save"
                    backgroundColor: "#2ecc71"
                    backgroundColorPressed: "#27ae60"
                    onClickAction: {
                        saveYamlConfig(yamlEditor.text)
                        yamlEditorDialog.close()
                    }
                }
            }
        }
    }

    // Confirmation dialogs
    Dialog {
        id: confirmRestartDialog
        title: "Confirm Restart"
        modal: true
        anchors.centerIn: parent

        ColumnLayout {
            spacing: 20

            Text {
                text: "Are you sure you want to restart the device?"
                font.pixelSize: root.scaled(16)
                Layout.preferredWidth: root.scaled(350)
                wrapMode: Text.WordWrap
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignRight

                Components.DialogButton {
                    label: "Cancel"
                    onClickAction: confirmRestartDialog.close()
                }

                Components.DialogButton {
                    label: "Restart"
                    backgroundColor: "#e74c3c"
                    backgroundColorPressed: "#c0392b"
                    onClickAction: {
                        restartDevice()
                        confirmRestartDialog.close()
                    }
                }
            }
        }
    }

    // Factory reset confirmation/warning dialog
    Dialog {
        id: confirmFactoryResetDialog
        title: "FACTORY RESET WARNING"
        modal: true
        anchors.centerIn: parent

        ColumnLayout {
            spacing: 20

            Text {
                text: "This will delete all data and reset smarthome application to default settings!"
                font.pixelSize: root.scaled(16)
                font.bold: true
                color: "#e74c3c"
                wrapMode: Text.WordWrap
                Layout.preferredWidth: root.scaled(350)
            }

            Text {
                text: "This action cannot be undone!"
                font.pixelSize: root.scaled(14)
                color: "#7f8c8d"
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignRight

                Components.DialogButton {
                    label: "Cancel"
                    onClickAction:confirmFactoryResetDialog.close()
                }

                Components.DialogButton {
                    label: "Factory Reset"
                    backgroundColor: "#e74c3c"
                    backgroundColorPressed: "#c0392b"
                    onClickAction: {
                        factoryReset()
                        confirmFactoryResetDialog.close()
                    }
                }
            }
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Item {
            width: settingsPage.width
            implicitHeight: mainLayout.implicitHeight

            ColumnLayout {
                id: mainLayout
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                spacing: 24

                // Network section
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: networkSection.implicitHeight + 40
                    color: "#ecf0f1"
                    radius: 12

                    ColumnLayout {
                        id: networkSection
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        // Section header
                        Text {
                            text: "Network"
                            font.pixelSize: root.scaled(24)
                            font.bold: true
                            color: "#2c3e50"
                        }

                        // Current WLAN connection
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: currentConnectionLayout.implicitHeight + 30
                            color: currentSSID ? "#2ecc71" : "#e74c3c"
                            radius: 8

                            ColumnLayout {
                                id: currentConnectionLayout
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 8

                                // Current WLAN ssid
                                Text {
                                    text: currentSSID ? "Connected to: " + currentSSID : "Not connected"
                                    font.pixelSize: root.scaled(16)
                                    font.bold: true
                                    color: "white"
                                }

                                // WLAN disconnect button
                                Button {
                                    text: "Disconnect"
                                    visible: currentSSID !== ""
                                    Layout.preferredWidth: root.scaled(150)
                                    Layout.preferredHeight: root.scaled(50)

                                    background: Rectangle {
                                        color: parent.pressed ? "#c0392b" : "#e74c3c"
                                        radius: 6
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        color: "white"
                                        font.pixelSize: root.scaled(16)
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: disconnectWifi()
                                }
                            }
                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }

                        // New WLAN connection
                        Text {
                            text: "Connect to Wi-Fi:"
                            font.pixelSize: root.scaled(16)
                            color: "#2c3e50"
                            font.bold: true
                        }

                        // WLAN ssid text field
                        TextField {
                            id: ssidField
                            placeholderText: "SSID"
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.scaled(60)
                            font.pixelSize: root.scaled(16)
                        }

                        // WLAN password text field
                        TextField {
                            id: passwordField
                            placeholderText: "Password"
                            echoMode: TextInput.Password
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.scaled(60)
                            font.pixelSize: root.scaled(16)
                        }

                        // WLAN connect button
                        Components.SettingsButton {
                            label: "Connect"
                            backgroundColor: "#2ecc71"
                            backgroundColorPressed: "#27ae60"
                            boldLabel: false
                            onClickAction: {
                                if (ssidField.text && passwordField.text) {
                                    connectToWifi(ssidField.text, passwordField.text)
                                    ssidField.text = ""
                                    passwordField.text = ""
                                }
                            }
                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }

                        // Network info
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 12
                            columnSpacing: 15

                            // IP address text
                            Text {
                                text: "IP address:"
                                font.pixelSize: root.scaled(14)
                                color: "#7f8c8d"
                            }

                            // IP address value
                            Text {
                                text: ipAddress
                                font.pixelSize: root.scaled(14)
                                font.family: "monospace"
                                color: "#2c3e50"
                            }

                            // MAC address text
                            Text {
                                text: "MAC address:"
                                font.pixelSize: root.scaled(14)
                                color: "#7f8c8d"
                            }

                            // MAC address value
                            Text {
                                text: macAddress
                                font.pixelSize: root.scaled(14)
                                font.family: "monospace"
                                color: "#2c3e50"
                            }

                            // Hostname text
                            Text {
                                text: "Hostname:"
                                font.pixelSize: root.scaled(14)
                                color: "#7f8c8d"
                            }

                            RowLayout {
                                spacing: 10

                                // Hostname value
                                Text {
                                    text: hostname
                                    font.pixelSize: root.scaled(14)
                                    font.family: "monospace"
                                    color: "#2c3e50"
                                }

                                // Hostname edit button
                                Button {
                                    text: "Edit"
                                    Layout.preferredWidth: root.scaled(80)
                                    Layout.preferredHeight: root.scaled(40)

                                    background: Rectangle {
                                        color: parent.pressed ? "#2980b9" : "#3498db"
                                        radius: 6
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        color: "white"
                                        font.pixelSize: root.scaled(14)
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: hostnameDialog.open()
                                }
                            }
                        }
                    }
                }

                // Display section
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: displaySection.implicitHeight + 40
                    color: "#ecf0f1"
                    radius: 12

                    ColumnLayout {
                        id: displaySection
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        // Section header
                        Text {
                            text: "Display"
                            font.pixelSize: root.scaled(24)
                            font.bold: true
                            color: "#2c3e50"
                        }

                        // Brightness slider
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "Brightness:"
                                    font.pixelSize: root.scaled(16)
                                    color: "#2c3e50"
                                }

                                Text {
                                    text: brightness + "%"
                                    font.pixelSize: root.scaled(16)
                                    font.bold: true
                                    color: "#3498db"
                                }
                            }

                            Slider {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.scaled(60)
                                to: 100
                                value: brightness
                                stepSize: 5

                                handle: Rectangle {
                                    x: parent.visualPosition * (parent.width - width)
                                    y: (parent.height - height) / 2
                                    width: root.scaled(40)
                                    height: root.scaled(40)
                                    radius: root.scaled(20)
                                    color: parent.pressed ? "#2980b9" : "#3498db"
                                }

                                background: Rectangle {
                                    width: parent.width
                                    height: root.scaled(12)
                                    y: (parent.height - height) / 2
                                    radius: root.scaled(6)
                                    color: "#bdc3c7"

                                    Rectangle {
                                        width: parent.parent.visualPosition * parent.width
                                        height: parent.height
                                        radius: parent.radius
                                        color: "#3498db"
                                    }
                                }

                                onValueChanged: setBrightness(value)
                            }
                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }

                        // Screensaver timeout
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 15

                            Text {
                                text: "Screensaver timeout:"
                                font.pixelSize: root.scaled(16)
                                color: "#2c3e50"
                                Layout.fillWidth: true
                            }

                            SpinBox {
                                from: 1
                                to: 60
                                value: screensaverTimeout
                                editable: true
                                font.pixelSize: root.scaled(16)
                                Layout.preferredHeight: root.scaled(60)

                                onValueChanged: setScreensaverTimeout(value)

                                textFromValue: function (value) {
                                    return value + " min"
                                }
                            }
                        }
                    }
                }

                // System section
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: systemSection.implicitHeight + 40
                    color: "#ecf0f1"
                    radius: 12

                    ColumnLayout {
                        id: systemSection
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        // Section header
                        Text {
                            text: "System"
                            font.pixelSize: root.scaled(24)
                            font.bold: true
                            color: "#2c3e50"
                        }

                        Components.SettingsButton {
                            label: "Reload config"
                            backgroundColor: "#3498db"
                            backgroundColorPressed: "#2980b9"
                            boldLabel: false
                            onClickAction: reloadConfig()
                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }


                        Components.SettingsButton {
                            label: "Restart device"
                            backgroundColor: "#e67e22"
                            backgroundColorPressed: "#d35400"
                            boldLabel: false
                            onClickAction: confirmRestartDialog.open()
                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }

                        Components.SettingsButton {
                            label: "Check and install system updates"
                            backgroundColor: "#2ecc71"
                            backgroundColorPressed: "#27ae60"
                            boldLabel: false
                            onClickAction: checkAndInstallUpdates()

                        }

                    }
                }

                // Advanced section
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: advancedSection.implicitHeight + 40
                    color: "#ecf0f1"
                    radius: 12

                    ColumnLayout {
                        id: advancedSection
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20

                        // Section header
                        Text {
                            text: "Advanced"
                            font.pixelSize: root.scaled(24)
                            font.bold: true
                            color: "#2c3e50"
                        }

                        // SSH toggle
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 15

                            Text {
                                text: "SSH Access:"
                                font.pixelSize: root.scaled(16)
                                color: "#2c3e50"
                                Layout.fillWidth: true
                            }

                            Switch {
                                checked: sshEnabled
                                Layout.preferredHeight: root.scaled(50)
                                scale: 1.5

                                onToggled: toggleSSH(checked)
                            }

                            Text {
                                text: sshEnabled ? "Enabled" : "Disabled"
                                font.pixelSize: root.scaled(16)
                                font.bold: true
                                color: sshEnabled ? "#2ecc71" : "#7f8c8d"
                            }
                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }

                        Components.SettingsButton {
                            label: "Edit YAML configuration"
                            backgroundColor: "#3498db"
                            backgroundColorPressed: "#2980b9"
                            boldLabel: false
                            onClickAction: {
                                yamlEditor.text = yamlConfig
                                yamlEditorDialog.open()
                            }

                        }

                        // Spacer bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#bdc3c7"
                        }

                        Components.SettingsButton {
                            label: "Factory reset"
                            backgroundColor: "#e74c3c"
                            backgroundColorPressed: "#c0392b"
                            boldLabel: true
                            onClickAction: confirmFactoryResetDialog.open()

                        }

                        // Factory reset warning text
                        Text {
                            text: "Factory reset will erase all data and settings"
                            font.pixelSize: root.scaled(12)
                            color: "#7f8c8d"
                            font.italic: true
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}