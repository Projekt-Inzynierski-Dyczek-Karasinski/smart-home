import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: detailPage

    property var moduleData: null

    signal backClicked()

    function executeAction(moduleId, method) {
        const notification = {
            "jsonrpc": "2.0",
            "method": "execute",
            "params": {
                "target": "core",
                "method_params": [method, {"module_id": moduleId}]
            },
        }

        apiClient.sendNotification(JSON.stringify(notification))
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 0


        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: root.height * 0.1
            Text {
                text: "Return"
                anchors.fill: parent
                font.pixelSize: root.scaled(16)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: detailPage.backClicked()
            }

        }

        ScrollView {
            id: scrollView

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Item {
                width: detailPage.width
                implicitHeight: columnLayout.implicitHeight

                ColumnLayout {
                    id: columnLayout
                    anchors.fill: parent
                    anchors.leftMargin: 32
                    anchors.rightMargin: 32
                    spacing: 20

                    // Module Name
                    Text {
                        text: moduleData ? moduleData.name : "Undefined"
                        font.pixelSize: root.scaled(32)
                        font.bold: true
                        color: "#2c3e50"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    // Module Description
                    Text {
                        text: moduleData ? moduleData.description : ""
                        font.pixelSize: root.scaled(16)
                        color: "#7f8c8d"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    // Spacer Bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 2
                        color: "#000000"
                    }

                    // Values
                    Text {
                        text: "Values"
                        font.pixelSize: root.scaled(24)
                        font.bold: true
                        color: "#2c3e50"
                    }

                    Repeater {
                        model: moduleData ? moduleData.values : []

                        Rectangle {
                            Layout.fillWidth: true
                            height: 60
                            color: "#ecf0f1"
                            radius: 8

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 15

                                Text {
                                    text: modelData.name
                                    font.pixelSize: root.scaled(18)
                                    color: "#2c3e50"
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.value
                                    font.pixelSize: root.scaled(20)
                                    font.bold: true
                                    color: "#3498db"
                                }
                            }
                        }
                    }

                    // Actions
                    Text {
                        text: "Actions"
                        font.pixelSize: root.scaled(24)
                        font.bold: true
                        color: "#2c3e50"
                    }

                    Repeater {
                        model: moduleData ? moduleData.actions : []

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.scaled(70)
                            text: modelData.name

                            background: Rectangle {
                                color: "#2ecc71"
                                radius: 8
                            }

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: root.scaled(20)
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    executeAction(moduleData.id, modelData.method)
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


}