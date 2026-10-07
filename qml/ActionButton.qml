import QtQuick
import QtQuick.Controls
Button {
 id: control
 property bool primary: false
 property bool danger: false
 implicitHeight: 38
 implicitWidth: Math.max(110, label.implicitWidth + 34)
 font.pixelSize: 12
 font.bold: true
 contentItem: Text { id: label; text: control.text; font: control.font; color: control.enabled ? "#eef6fc" : "#74818e"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
 background: Rectangle { radius: 10; color: !control.enabled ? "#17222e" : control.down ? "#243e55" : control.danger ? "#633728" : control.primary ? "#245c84" : control.hovered ? "#23364a" : "#172535"; border.color: control.danger ? "#bb7759" : control.primary ? "#62b5e9" : "#455565"; border.width: 1 }
}
