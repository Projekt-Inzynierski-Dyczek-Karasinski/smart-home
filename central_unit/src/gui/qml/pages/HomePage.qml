import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: homePage

    signal moduleClicked(var moduleData)

    property var modulesData: []

    Component.onCompleted: {
        fetchModules()
    }

    onVisibleChanged: {
        if (visible) {
            fetchModules()
        }
    }

    function fetchModules() {
        const request = {
            "jsonrpc": "2.0",
            "method": "get",
            "params": {"target": "core", "method_params": ["modules_status"]},
            "id": root.requestIdCounter++
        }

        const response = apiClient.sendRequest(JSON.stringify(request))
        const data = JSON.parse(response)

        if (data.result) {
            modulesData = data.result
        } else {
            console.error("Modules status fetch failed - " + data.error.toString())
        }
    }

    function executeAction(moduleId, method) {
        const notification = {
            "jsonrpc": "2.0",
            "method": "execute",
            "params": {
                "target": "core",
                "method_params": [method, {"module_id": moduleId}]
            }
        }

        const response = apiClient.sendNotification(JSON.stringify(notification))

        // Update data after action
        fetchModules() //TODO change to singular module update
    }


    GridView {
        id: grid
        anchors.fill: parent
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        clip: true

        cellWidth: width / 3
        cellHeight: height / 2 * 0.8
        model: modulesData

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        // Spacer
        header: Item {
            width: grid.width
            height: 16
        }

        // Spacer
        footer: Item {
            width: grid.width
            height: 16
        }


        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            property var moduleItem: modelData

            Button {
                width: parent.width - 32
                height: parent.height - 32
                anchors.centerIn: parent
                clip: true


                background: Rectangle {
                    color: "#2c3e50"
                    radius: 10
                }

                //TODO test scrollable module cards on touch screen, if scrolling doesn't work make cards static
                contentItem: ScrollView {
                    id: scrollView
                    anchors.fill: parent
                    anchors.margins: 15
                    clip: true

                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Item {
                        width: scrollView.width - scrollView.ScrollBar.vertical.width
                        implicitHeight: columnLayout.implicitHeight

                        ColumnLayout {
                            id: columnLayout
                            anchors.fill: parent
                            spacing: 8

                            // Module Name
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: headerText.implicitHeight
                                color: "transparent"

                                Text {
                                    id: headerText
                                    anchors.fill: parent
                                    text: moduleItem.name || "Undefined"
                                    color: "#ffffff"
                                    font.pixelSize: root.scaled(18)
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        homePage.moduleClicked(moduleItem)
                                    }
                                }
                            }

                            // Module Description
                            Text {
                                text: moduleItem.description || ""
                                color: "#95a5a6"
                                font.pixelSize: root.scaled(12)
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }

                            // Spacer Bar
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#34495e"
                            }

                            // Module Values
                            Repeater {
                                model: moduleItem.values

                                Row {
                                    spacing: 5
                                    Layout.fillWidth: true

                                    Text {
                                        text: modelData.name + ":"
                                        color: "#7f8c8d"
                                        font.pixelSize: root.scaled(13)
                                    }
                                    Text {
                                        text: modelData.value
                                        color: "white"
                                        font.pixelSize: root.scaled(13)
                                        font.bold: true
                                    }
                                }
                            }

                            // Spacer
                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 10
                            }

                            // Module Actions
                            Repeater {
                                model: moduleItem.actions

                                RowLayout {
                                    spacing: 15
                                    Layout.fillWidth: true
                                    Layout.topMargin: 4

                                    Text {
                                        text: modelData.name
                                        color: "#ecf0f1"
                                        font.pixelSize: root.scaled(12)
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                    Button {
                                        text: ">>>"
                                        Layout.preferredWidth: root.scaled(60)
                                        Layout.preferredHeight: root.scaled(40)

                                        background: Rectangle {
                                            color: parent.pressed ? "#27ae60" : "#2ecc71"
                                            radius: 5
                                        }

                                        contentItem: Text {
                                            text: parent.text
                                            color: "white"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            font.pixelSize: root.scaled(16)
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                executeAction(moduleItem.id, modelData.method)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}