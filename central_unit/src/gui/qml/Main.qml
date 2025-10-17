import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "components" as Components
import "pages" as Pages

ApplicationWindow {
    readonly property real baseHeight: 480

    id: root
    visible: true
    minimumWidth: 800
    minimumHeight: baseHeight
    title: "Smarthome GUI"
    visibility: "FullScreen"

    property real scaleFactor: root.height / baseHeight

    function scaled(value){
        return value * scaleFactor
    }

    property int requestIdCounter: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Nav
        TabBar {
            id: tabBar
            Layout.fillWidth: true

            Components.CustomTabButton {
                label: "Home"
            }
            Components.CustomTabButton {
                label: "Status"
            }
            Components.CustomTabButton {
                label: "Settings"
            }

        }

        // Main
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // HomePage
            Item {
                StackView {
                    id: homeStack
                    anchors.fill: parent
                    initialItem: homePageComponent

                    Component {
                        id: homePageComponent

                        Pages.HomePage {
                            id: homePage
                            onModuleClicked: function (moduleData) {
                                homeStack.push(moduleDetailComponent, {
                                    moduleData: moduleData
                                })
                            }
                        }
                    }

                    Component {
                        id: moduleDetailComponent

                        Pages.ModuleDetailPage {
                            onBackClicked: {
                                homeStack.pop()
                            }
                        }
                    }
                }
            }

            Pages.StatusPage {
            }

            Pages.SettingsPage {
            }

        }
    }
}