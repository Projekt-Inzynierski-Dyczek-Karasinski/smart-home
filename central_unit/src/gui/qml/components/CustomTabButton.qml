import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TabButton {
    id: control
    implicitHeight: root.scaled(64)
    property alias label: buttonText.text

    background: Rectangle {
        color: control.checked ? "#34495e" : "#2c3e50"
    }

    contentItem: Text {
        id: buttonText
        anchors.fill: parent
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: "#ffffff"
        font.pixelSize: root.scaled(32)
    }
}