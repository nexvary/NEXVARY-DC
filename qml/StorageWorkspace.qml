import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Panel {
 id: pane
 required property var app
 property bool bootMode: false
 property var disk: app.selectedDisk
 function prepare(action) {
  if(!disk.device) {app.notice=app.t("اختر قرصًا أولًا","Select a disk first");return}
  let p=partitionBox.currentIndex>=0 && partitionBox.currentIndex<(disk.partitions||[]).length?disk.partitions[partitionBox.currentIndex]:{}
  const plan=backend.prepareStorage(disk.device,action,p.partition||0,fs.currentText,style.currentText,partitionSize.value,isoPath.text)
  if(plan.error) {app.notice=plan.error;return}
  app.confirmPlan=plan;app.confirmText="";app.confirmAck=false;app.confirmation.open()
 }
 ColumnLayout {anchors.fill:parent;spacing:12
  RowLayout {AppIcon {name:pane.bootMode?"boot":"partition";Layout.preferredWidth:44;Layout.preferredHeight:44}Label {text:pane.bootMode?app.t("تجهيز وسيط تثبيت Windows","Create Windows installation media"):app.t("إدارة الأقسام ونظام الملفات","Partitions and filesystem management");font.pixelSize:20;font.bold:true;color:"#eff5fa";Layout.fillWidth:true;wrapMode:Text.WordWrap} }
  Label {text:app.t("الكتابة متاحة على Windows لأجهزة USB / SD / MMC الخارجية فقط. الأقراص الداخلية وأقراص النظام محمية.","Windows writes support external USB / SD / MMC devices only. Internal and system disks are protected.");color:"#b2c5d5";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {visible:!backend.windows;text:app.t("إدارة الأقراص لهذه النسخة متاحة على Windows؛ التشخيص وفحص الصور يعملان على Linux.","Disk management in this version requires Windows; diagnostics and image inspection work on Linux.");color:"#f0bc71";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  RowLayout {visible:backend.windows&&!backend.administrator;Layout.fillWidth:true
   Label {text:app.t("عمليات الصيانة تحتاج صلاحيات المسؤول","Maintenance requires administrator permission");color:"#edc178";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   ActionButton {text:app.t("تشغيل كمسؤول","Run as administrator");enabled:!backend.busy;onClicked:backend.relaunchAdministrator()}
  }
  Rectangle {Layout.fillWidth:true;implicitHeight:warning.implicitHeight+24;radius:8;color:"#302319";border.color:"#80533c"
   Label {id:warning;anchors.fill:parent;anchors.margins:12;text:app.t("الفورمات وحذف الأقسام وتجهيز الإقلاع تحذف البيانات. احتفظ بنسخة خارج الوسيط المستهدف. لن يبدأ التنفيذ قبل مراجعة الجهاز والتأكيد.","Formatting, deleting partitions and creating boot media erase data. Back up outside the target device. Execution requires device review and confirmation.");color:"#ffcd9e";wrapMode:Text.WordWrap}
  }
  ColumnLayout {visible:!pane.bootMode;Layout.fillWidth:true;spacing:10
   Label {text:app.t("القسم المستهدف","Target partition");color:"#9dbacf"}
   ComboBox {id:partitionBox;Layout.fillWidth:true;model:(pane.disk.partitions||[]).map(p=>"#"+p.partition+" · "+(p.letter?p.letter+":":"—")+" · "+app.size(p.partitionBytes)+" · "+(p.filesystem||"RAW"));enabled:!backend.busy}
   RowLayout {Label {text:app.t("نظام الملفات","Filesystem");color:"#bbd0df"}ComboBox {id:fs;model:["exFAT","NTFS","FAT32"];enabled:!backend.busy}Item {Layout.fillWidth:true}ActionButton {text:app.t("فورمات القسم","Format partition");danger:true;enabled:!backend.busy&&backend.windows;onClicked:pane.prepare("format")} }
   Flow {Layout.fillWidth:true;spacing:8
    ActionButton {text:app.t("فحص نظام الملفات","Scan filesystem");enabled:!backend.busy&&backend.windows;onClicked:pane.prepare("check")}
    ActionButton {text:app.t("إصلاح نظام الملفات","Repair filesystem");danger:true;enabled:!backend.busy&&backend.windows;onClicked:pane.prepare("repair")}
    ActionButton {text:app.t("حذف القسم","Delete partition");danger:true;enabled:!backend.busy&&backend.windows;onClicked:pane.prepare("delete")}
   }
   Rectangle {Layout.fillWidth:true;height:1;color:"#2d455a"}
   RowLayout {Label {text:app.t("قسم جديد في المساحة غير المخصصة (MiB)","New partition in unallocated space (MiB)");color:"#bfd0de";Layout.fillWidth:true;wrapMode:Text.WordWrap}SpinBox {id:partitionSize;from:16;to:2097152;value:1024;editable:true;enabled:!backend.busy} }
   ActionButton {text:app.t("إنشاء القسم وتهيئته","Create and format partition");enabled:!backend.busy&&backend.windows;onClicked:pane.prepare("create")}
   RowLayout {ComboBox {id:style;model:["GPT","MBR"];enabled:!backend.busy}ActionButton {text:app.t("مسح التقسيم وإنشاء قسم واحد","Erase layout and create one partition");danger:true;enabled:!backend.busy&&backend.windows;onClicked:pane.prepare("layout")} }
   Label {text:app.t("تحويل GPT / MBR هنا يمسح التقسيم والبيانات؛ لا يُقدَّم كتحويل يحافظ على الملفات.","GPT / MBR conversion here erases the layout and data; it is not a data-preserving conversion.");color:"#e7bc7b";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  }
  ColumnLayout {visible:pane.bootMode;Layout.fillWidth:true;spacing:12
   Label {text:app.t("Windows x64 • UEFI • GPT / FAT32","Windows x64 • UEFI • GPT / FAT32");color:"#6ed9b1";font.bold:true}
   Label {text:app.t("يُفحص ملف ISO قبل المسح. تُنسخ ملفات التثبيت ويُتحقق منها بـSHA-256. يُقسّم install.wim الكبير بواسطة DISM. القسم بحد أقصى 31 GiB. إقلاع BIOS وصور Linux ليسا مدعومين بهذا المسار.","ISO validation happens before erasure. Installation files are copied and SHA-256 checked. Large install.wim files are split with DISM. Partition maximum: 31 GiB. BIOS boot and Linux ISOs are not supported by this path.");color:"#b1c7d8";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   RowLayout {Layout.fillWidth:true;TextField {id:isoPath;Layout.fillWidth:true;placeholderText:app.t("مسار ISO الخاص بـWindows","Windows ISO path");enabled:!backend.busy;selectByMouse:true}ActionButton {text:app.t("اختيار ISO","Browse ISO");onClicked:app.chooseIso(function(path){isoPath.text=path})} }
   ActionButton {text:app.t("مسح الوسيط وتجهيز التثبيت","Erase media and create installer");danger:true;enabled:!backend.busy&&backend.windows&&isoPath.text.length>0;onClicked:pane.prepare("windows_usb")}
  }
 }
}
