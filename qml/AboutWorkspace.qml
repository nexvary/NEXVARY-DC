import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Panel {
 required property var app
 ColumnLayout {
  anchors.fill: parent; spacing: 16
  RowLayout {spacing:16;AppIcon {name:"brand";Layout.preferredWidth:84;Layout.preferredHeight:84}
   ColumnLayout {Layout.fillWidth:true
    Label {text:"NEXVARY Disk Care";font.pixelSize:26;font.bold:true;color:"#eaf2fa"}
    Label {text:app.t("تشخيص وصيانة وإدارة وسائط التخزين","Storage diagnostics, maintenance and management");color:"#8fc9eb";Layout.fillWidth:true;wrapMode:Text.WordWrap}
    Label {text:"0.2.0 · C++20 / Qt 6";color:"#9eb2c4"}
   }
  }
  Rectangle {Layout.fillWidth:true;height:1;color:"#334c60"}
  Label {text:app.t("بسم الله الرحمن الرحيم","In the name of Allah, the Most Gracious, the Most Merciful");color:"#68d9b0";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {text:app.t("المطور الرئيسي: علاء محمد","Lead developer: Alaa Mohamed");font.pixelSize:22;font.bold:true;color:"#efc983"}
  Label {text:app.t("هوية المنتج: NEXVARY • بيانات المطور وروابط التواصل من صفحة FG Machines المعتمدة في FG MTM.","Product identity: NEXVARY • Developer details and contact links from the FG Machines developer page used in FG MTM.");color:"#b6cadb";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {text:app.t("برنامج مجاني لوجه الله تعالى. هدفنا أدوات واضحة، ونتائج قابلة للتحقق، ومراجعة قبل أي تغيير حساس.","A free application for the sake of Allah. Clear tools, verifiable results and review before sensitive changes.");color:"#b6cadb";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Flow {Layout.fillWidth:true;spacing:10
   ActionButton {text:"nexvary.com";primary:true;onClicked:Qt.openUrlExternally("https://nexvary.com")}
   ActionButton {text:"fgmachines.org";onClicked:Qt.openUrlExternally("https://fgmachines.org")}
   ActionButton {text:app.t("صفحة المطور","Developer Facebook");onClicked:Qt.openUrlExternally("https://www.facebook.com/share/1EKVAyZZ2C/")}
   ActionButton {text:app.t("الصفحة التجارية","Business Facebook");onClicked:Qt.openUrlExternally("https://www.facebook.com/share/1T7r3WpH8Y/")}
  }
  Label {text:app.t("لا يمكن للبرمجيات تجديد سطح تالف ماديًا. نتائج الاختبار توضّح نطاق الفحص؛ نجاح SMART وحده لا يضمن سلامة البيانات.","Software cannot regenerate a physically damaged surface. Reports state test scope; passing SMART alone does not guarantee data safety.");color:"#e1ba75";Layout.fillWidth:true;wrapMode:Text.WordWrap}
 }
}
