import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button{
    id: button
    property alias label: buttonText.text
    property color backgroundColor: "#000000"
    property color backgroundColorPressed: "#000000"
    property alias boldLabel: buttonText.font.bold
    signal clickAction()

    Layout.fillWidth: true
    Layout.preferredHeight: root.scaled(60)

    background: Rectangle {
        id: buttonBackground
        color: parent.pressed ? button.backgroundColorPressed : button.backgroundColor
        radius: 8
    }

    contentItem: Text {
        id: buttonText
        color: "white"
        font.pixelSize: root.scaled(16)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    onClicked: clickAction()
}