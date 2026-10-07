import QtQuick
Image {
 property string name: "disk"
 source: "qrc:/assets/icons/"+name+".png"
 fillMode: Image.PreserveAspectFit
 smooth: true
 sourceSize.width: 128; sourceSize.height: 128
}
