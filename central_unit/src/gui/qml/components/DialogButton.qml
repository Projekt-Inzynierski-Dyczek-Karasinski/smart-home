import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button{
    id: button
    property alias label: buttonText.text
    property color backgroundColor: "#AAAAAA"
    property color backgroundColorPressed: Qt.darker(backgroundColor, 1.2)
    signal clickAction()

    Layout.preferredWidth: root.scaled(120)
    Layout.preferredHeight: root.scaled(50)
    font.pixelSize: root.scaled(16)

    background: Rectangle {
        id: buttonBackground
        color: parent.pressed ? button.backgroundColorPressed : button.backgroundColor
        border.width: 1
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