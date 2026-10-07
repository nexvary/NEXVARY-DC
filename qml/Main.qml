import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ApplicationWindow {
 id: root
 width: 1280; height: 850; minimumWidth: 980; minimumHeight: 700
 visible: true; title: "NEXVARY Disk Care · 0.1.0"
 color: "#09121c"
 property bool arabic: true
 property int page: 0
 property string historyReport: ""
 property string notice: ""
 function t(ar, en) { return arabic ? ar : en }
 function size(bytes) { return (Number(bytes) / 1073741824).toFixed(2) + " GiB" }
 property var names: [t("نظرة عامة","Overview"),t("تشخيص الأقراص","Disk diagnostics"),t("إنقاذ البيانات","Data rescue"),t("الصيانة وFirmware","Maintenance & firmware"),t("الفلاشات والذاكرة","Flash & memory cards"),t("كشف السعة المزيفة","Capacity verification"),t("التقارير والسجل","Reports & history")]
 LayoutMirroring.enabled: arabic
 LayoutMirroring.childrenInherit: true
 font.family: Qt.platform.os === "windows" ? "Segoe UI" : "DejaVu Sans"
 palette.text: "#ecf3f9"; palette.windowText: "#ecf3f9"; palette.base: "#0b1723"; palette.button: "#1d3449"; palette.highlight: "#3b91cf"; palette.buttonText: "#ecf3f9"
 FileDialog { id: sourceDialog; title: root.t("اختر ملف صورة قرص","Choose a disk image file"); onAccepted: imagePath.text = backend.localPath(selectedFile.toString()) }
 FileDialog { id: targetDialog; fileMode: FileDialog.SaveFile; title: root.t("ملف جديد لنسخة الصورة","New image copy file"); onAccepted: targetPath.text = backend.localPath(selectedFile.toString()) }
 FileDialog { id: reportDialog; fileMode: FileDialog.SaveFile; nameFilters: ["JSON (*.json)"]; onAccepted: root.notice = backend.exportReport(backend.localPath(selectedFile.toString())) ? root.t("تم حفظ التقرير","Report saved") : root.t("تعذر الحفظ؛ اختر اسمًا جديدًا وتحقق من الصلاحيات","Save failed; use a new filename and check permissions") }
 FolderDialog { id: folderDialog; onAccepted: capacityPath.text = backend.localPath(selectedFolder.toString()) }
 RowLayout {
  anchors.fill: parent; anchors.margins: 20; spacing: 20
  Rectangle {
   Layout.preferredWidth: 235; Layout.fillHeight: true; color: "#101d2b"; radius: 18; border.color: "#34495c"
   ColumnLayout {
    anchors.fill: parent; anchors.margins: 18; spacing: 12
    RowLayout { spacing: 12; DiskIcon { Layout.preferredWidth: 40; Layout.preferredHeight: 40 }
     ColumnLayout { spacing: 0; Label {text:"NEXVARY";font.bold:true;font.pixelSize:22;color:"#eaf2f9"} Label {text:"DISK CARE";font.pixelSize:12;color:"#82b9de";font.letterSpacing:2} }
    }
    Rectangle { Layout.fillWidth:true;height:1;color:"#354859";Layout.topMargin:14;Layout.bottomMargin:10 }
    Repeater {
     model: root.names
     delegate: Button {
      required property string modelData
      required property int index
      Layout.fillWidth:true;implicitHeight:52
      onClicked: {root.page=index;root.notice=""}
      background: Rectangle {radius:10;color:root.page===index?"#213c55":parent.hovered?"#1b2d40":"transparent";border.color:root.page===index?"#629abe":"transparent"}
      contentItem: RowLayout {spacing:12;DiskIcon {kind: index===0?0:index-1;width:26;height:26;Layout.preferredWidth:26;Layout.preferredHeight:26;ink:root.page===index?"#86cefa":"#839aaa"} Label {text:modelData;font.pixelSize:13;font.bold:root.page===index;color:"#ecf3f9";Layout.fillWidth:true} }
     }
    }
    Item {Layout.fillHeight:true}
    Label {text:root.t("نواة تجريبية · 0.1.0","Foundation preview · 0.1.0");color:"#91a9bb";font.pixelSize:12;Layout.fillWidth:true;wrapMode:Text.WordWrap}
    Label {text:root.t("العمليات الخام لم تُفعّل بعد","Raw write operations are not enabled yet");color:"#d9b96f";font.pixelSize:11;Layout.fillWidth:true;wrapMode:Text.WordWrap}
    ActionButton {Layout.fillWidth:true;text:root.arabic?"English":"العربية";onClicked:root.arabic=!root.arabic}
   }
  }
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true;spacing:16
   RowLayout {
    Layout.fillWidth:true
    ColumnLayout {spacing:5;Label {text:root.names[root.page];font.pixelSize:26;font.bold:true;color:"#f1f6fb"}
     Label {text:root.t("تشخيص واضح. إنقاذ موثق. صيانة قابلة للتحقق.","Clear diagnostics. Recorded rescue. Verifiable maintenance.");color:"#93aabd";font.pixelSize:12} }
    Item {Layout.fillWidth:true}
    ActionButton {visible:root.page!==0;text:root.t("رجوع للرئيسية","Back to overview");onClicked:root.page=0}
    ActionButton {text:root.t("تحديث الأقراص","Refresh devices");enabled:!backend.busy;onClicked:backend.refresh()}
   }
   Panel {
    Layout.fillWidth:true;padding:14
    RowLayout {anchors.fill:parent;spacing:14
     Rectangle {width:10;height:10;radius:5;color:backend.busy?"#e3bd6b":"#74d5bb"}
     Label {text:backend.busy?root.t("عملية قيد التنفيذ","Operation in progress"):root.t("جاهز · اختر قرصًا أو ملف صورة","Ready · choose a disk or image file");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#e5eef5"}
     ProgressBar {Layout.preferredWidth:160;value:backend.progress;visible:backend.busy}
     ActionButton {visible:backend.busy;text:root.t("إلغاء العملية","Cancel operation");onClicked:backend.cancel()}
    }
   }
   ScrollView {
    id: scroll
    Layout.fillWidth:true;Layout.fillHeight:true;clip:true
    contentWidth:availableWidth
    ColumnLayout {
     width:scroll.availableWidth;spacing:16
     Panel {
      visible:root.page===0;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:18
       RowLayout {DiskIcon {width:62;height:62;Layout.preferredWidth:62;Layout.preferredHeight:62;ink:"#90ceee"}
        ColumnLayout {Layout.fillWidth:true;Label {text:root.t("اعرف حالة وسيط التخزين قبل أي إجراء","Understand your storage before acting");font.pixelSize:22;font.bold:true;color:"#eaf4fc";Layout.fillWidth:true;wrapMode:Text.WordWrap}
         Label {text:root.t("ابدأ بالفحص، واحفظ بياناتك قبل اختبارات الكتابة.","Start with diagnostics and back up before write tests.");color:"#9db6c9";Layout.fillWidth:true;wrapMode:Text.WordWrap} }
       }
       RowLayout {Layout.fillWidth:true;spacing:12
        Repeater {model:[{n:backend.disks.length,s:root.t("أقراص مكتشفة","Discovered disks"),c:"#85c9ed"},{n:backend.volumes.length,s:root.t("وحدات مركبة","Mounted volumes"),c:"#75cbb3"},{n:backend.history.length,s:root.t("تقارير محفوظة","Saved reports"),c:"#dbbf7f"}]
         delegate: Rectangle {required property var modelData;Layout.fillWidth:true;height:92;radius:12;color:"#0b1723";border.color:"#2c4155"
          Column {anchors.centerIn:parent;spacing:6;Label {anchors.horizontalCenter:parent.horizontalCenter;text:modelData.n;font.pixelSize:28;font.bold:true;color:modelData.c}Label {text:modelData.s;color:"#a8bdcd";font.pixelSize:12} }
         }
        }
       }
      }
     }
     GridLayout {
      visible:root.page===0;Layout.fillWidth:true;columns:2;columnSpacing:14;rowSpacing:14
      Repeater {model:[{p:1,k:0,title:root.t("تشخيص الأقراص","Disk diagnostics"),desc:root.t("هوية القرص وSMART عند توفر المحرك","Disk identity and SMART when the engine is available"),color:"#81c4ee"},{p:2,k:1,title:root.t("فحص ونسخ الصور","Inspect & copy images"),desc:root.t("قراءة الملف والتحقق باستخدام SHA-256","Read image files and verify with SHA-256"),color:"#6fceb2"},{p:5,k:4,title:root.t("كشف السعة المزيفة","Verify storage capacity"),desc:root.t("اختبار كتابة وقراءة بمساحة تختارها","Write and read verification over a chosen test size"),color:"#d9bc76"},{p:6,k:5,title:root.t("التقارير والسجل","Reports & history"),desc:root.t("نتائج مفصلة وتصدير JSON","Detailed results and JSON export"),color:"#b4a0e5"}]
       delegate: Panel {required property var modelData;Layout.fillWidth:true;Layout.minimumHeight:175
        ColumnLayout {anchors.fill:parent;spacing:10;RowLayout {DiskIcon {kind:modelData.k;ink:modelData.color;Layout.preferredWidth:34;Layout.preferredHeight:34}Label {text:modelData.title;font.pixelSize:17;font.bold:true;color:"#e9f1f8";Layout.fillWidth:true;wrapMode:Text.WordWrap} }
         Label {text:modelData.desc;color:"#98afc2";Layout.fillWidth:true;wrapMode:Text.WordWrap}
         ActionButton {text:root.t("فتح القسم","Open section");onClicked:root.page=modelData.p}
        }
       }
      }
     }
     Panel {
      visible:root.page===1;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:14
       Label {text:root.t("الأقراص الفعلية المكتشفة","Discovered physical disks");font.pixelSize:19;font.bold:true;color:"#e7f1f9"}
       Label {visible:backend.disks.length===0;text:root.t("لم تُكتشف أقراص فعلية. راجع تقرير الاكتشاف؛ البيئات المعزولة قد لا تعرضها.","No physical disks discovered. Check discovery results; isolated environments may hide them.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#d6b875"}
       Repeater {model:backend.disks;delegate:Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:100;radius:12;color:"#0b1723";border.color:"#34495c"
        RowLayout {anchors.fill:parent;anchors.margins:14;DiskIcon {Layout.preferredWidth:35;Layout.preferredHeight:35}
         ColumnLayout {Layout.fillWidth:true;Label {text:modelData.model||modelData.device;color:"#e8f2fb";font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight}Label {text:modelData.device+" · "+root.size(modelData.bytes)+" · "+(modelData.transport||"—");color:"#9ab2c5";Layout.fillWidth:true;elide:Text.ElideRight}Label {text:root.t("الرقم التسلسلي: ","Serial: ")+(modelData.serial||"—");color:"#819aaf";font.pixelSize:11} }
         ActionButton {text:root.t("قراءة SMART","Read SMART");enabled:!backend.busy;onClicked:backend.inspectHealth(modelData.device)}
        }
       } }
       Label {text:root.t("قراءة فقط. غياب SMART لا يعني أن القرص سليم. فحص السطح الخام يأتي بعد اختبارات التوافق.","Read only. Missing SMART does not mean a healthy disk. Raw surface scanning follows compatibility tests.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#a2b8c9"}
      }
     }
     Panel {
      visible:root.page===2;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       Label {text:root.t("فحص ونسخ ملف صورة قرص","Inspect and copy a disk image file");font.pixelSize:19;font.bold:true;color:"#eaf3fa"}
       Label {text:root.t("متاح الآن: ملفات الصور العادية فقط. إنقاذ الهارد المتعثر واستعادة الملفات لم يُدمجا بعد.","Available now: regular image files only. Failing-drive imaging and file recovery are not integrated yet.");color:"#d6bc81";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       RowLayout {Layout.fillWidth:true;TextField {id:imagePath;color:"#e6f0f8";placeholderTextColor:"#809bad";implicitHeight:44;Layout.fillWidth:true;placeholderText:root.t("مسار ملف الصورة المصدر","Source image file path");enabled:!backend.busy;selectByMouse:true}ActionButton {text:root.t("اختيار الملف","Browse file");enabled:!backend.busy;onClicked:sourceDialog.open()} }
       RowLayout {Layout.fillWidth:true;TextField {id:targetPath;color:"#e6f0f8";placeholderTextColor:"#809bad";implicitHeight:44;Layout.fillWidth:true;placeholderText:root.t("مسار جديد للنسخة؛ لن يُستبدل ملف موجود","New destination path; existing files are refused");enabled:!backend.busy;selectByMouse:true}ActionButton {text:root.t("مكان النسخة","Copy destination");enabled:!backend.busy;onClicked:targetDialog.open()} }
       RowLayout {ActionButton {text:root.t("فحص وحساب SHA-256","Read & calculate SHA-256");primary:true;enabled:!backend.busy&&imagePath.text.length>0;onClicked:backend.scanImage(imagePath.text)}ActionButton {text:root.t("نسخ والتحقق","Copy & verify");enabled:!backend.busy&&imagePath.text.length>0&&targetPath.text.length>0;onClicked:backend.copyImage(imagePath.text,targetPath.text)} }
      }
     }
     Panel {
      visible:root.page===3||root.page===4;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:16
       DiskIcon {kind:root.page===3?2:3;ink:"#d9bf82";Layout.preferredWidth:48;Layout.preferredHeight:48}
       Label {text:root.page===3?root.t("صيانة حسب قدرات الجهاز","Maintenance based on device capabilities"):root.t("الفلاشات وكروت الميموري","Flash drives and memory cards");font.pixelSize:22;font.bold:true;color:"#edf4fa";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       Label {text:root.page===3?root.t("المسح الخام ومعالجة القطاعات وتحديث Firmware مخططة؛ لا توجد أوامر كتابة خام في هذه النسخة. سيتم تفعيلها لكل موديل بعد التحقق.","Raw erasure, sector treatment and firmware updates are planned. This build contains no raw write commands. Enablement will follow per-model verification."):root.t("متاح الآن: عرض الوحدات واختبار مساحة مختارة وتصدير النتائج. إصلاح RAW والأقسام واستعادة الملفات مخطط للمرحلة التالية.","Available now: mounted volumes, selected-space verification and exported results. RAW/partition repair and file recovery are planned for the next stage.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#a9bfd0";font.pixelSize:15}
       ActionButton {text:root.t("افتح اختبار السعة","Open capacity verification");visible:root.page===4;onClicked:root.page=5}
      }
     }
     Panel {
      visible:root.page===5;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:14
       Label {text:root.t("اختبر البيانات؛ لا تثق بالسعة المعلنة وحدها","Verify data, not just the reported capacity");font.pixelSize:20;font.bold:true;color:"#eef4fa";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       Label {text:root.t("اختبار ملفات في مجلد تختاره، وليس فحصًا للسعة الداخلية الكاملة. النتيجة تخص المساحة المختبرة فقط. ذاكرة النظام المؤقتة قد تؤثر في قوة التحقق.","A file test in a selected folder, not a full internal-capacity probe. Results apply only to tested space. Operating-system caching may limit verification strength.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#a1b8ca"}
       ComboBox {id:volumeCombo;Layout.fillWidth:true;model:backend.volumes;textRole:"root";onActivated:capacityPath.text=backend.volumes[currentIndex].root;enabled:!backend.busy}
       RowLayout {Layout.fillWidth:true;TextField {id:capacityPath;color:"#e6f0f8";placeholderTextColor:"#809bad";implicitHeight:44;Layout.fillWidth:true;placeholderText:root.t("مجلد على الفلاشة أو الكارت","Folder on the flash drive or card");enabled:!backend.busy;selectByMouse:true}ActionButton {text:root.t("اختر المجلد","Choose folder");enabled:!backend.busy;onClicked:folderDialog.open()} }
       RowLayout {Label {text:root.t("مساحة الاختبار (MiB)","Test size (MiB)");color:"#e5edf4"}SpinBox {id:testSize;from:1;to:1048576;value:64;editable:true;enabled:!backend.busy}Label {text:root.t("احتياطي 64 MiB مطلوب","64 MiB free reserve required");color:"#a3b7c6";font.pixelSize:11} }
       Rectangle {Layout.fillWidth:true;implicitHeight:caution.implicitHeight+28;color:"#30291b";border.color:"#76633a";radius:10
        Label {id:caution;anchors.fill:parent;anchors.margins:14;text:root.t("انسخ ملفاتك أولًا. على وسيط مغشوش، الكتابة حتى في المساحة الفارغة قد تتلف ملفاتك الموجودة. تُحذف ملفات الاختبار الخاصة بنا بعد العملية.","Back up first. On counterfeit media, writing even to free space can corrupt existing files. Our test files are removed after the operation.");wrapMode:Text.WordWrap;color:"#edcd8d"} }
       CheckBox {id:ack;Layout.fillWidth:true;text:root.t("حفظت نسخة من البيانات وأوافق على اختبار الكتابة","I backed up my data and accept the write test");enabled:!backend.busy}
       ActionButton {text:root.t("ابدأ الكتابة ثم التحقق","Write, then verify");primary:true;enabled:!backend.busy&&ack.checked&&capacityPath.text.length>0;onClicked:backend.testCapacity(capacityPath.text,testSize.value,ack.checked)}
      }
     }
     Panel {
      visible:root.page===0||root.page===4;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12;Label {text:root.t("وحدات التخزين المركبة","Mounted storage volumes");font.pixelSize:18;font.bold:true;color:"#e8f1f8"}
       Repeater {model:backend.volumes;delegate:RowLayout {required property var modelData;Layout.fillWidth:true;Label {text:modelData.root;Layout.preferredWidth:180;color:"#83c3e9";elide:Text.ElideMiddle}Label {text:root.size(modelData.bytes);color:"#e5eef5";Layout.preferredWidth:120}Label {text:root.t("متاح ","Available ")+root.size(modelData.free);color:"#91b3c8";Layout.fillWidth:true}Label {text:modelData.filesystem;color:"#a0b4c5"} } }
      }
     }
     Panel {
      visible:root.page!==0&&root.page!==3&&root.page!==4;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       RowLayout {Layout.fillWidth:true;Label {text:root.t("نتيجة العملية الحالية","Current operation result");font.bold:true;font.pixelSize:17;color:"#e8f1f8";Layout.fillWidth:true}ActionButton {text:root.t("تصدير JSON","Export JSON");enabled:!backend.busy&&backend.report.length>0;onClicked:reportDialog.open()} }
       Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;font.bold:true;font.pixelSize:18;color:backend.result.status==="completed"?"#8cdbc1":"#e2bd79";text:backend.result.status==="completed"?root.t("اكتملت العملية ضمن نطاق الاختبار","Operation completed within its test scope"):backend.result.status==="mismatch"?root.t("اختلاف في البيانات: سعة مغشوشة أو عطل محتمل","Data mismatch: counterfeit capacity or storage fault suspected"):backend.result.status==="running"?root.t("جارٍ التنفيذ","Running"):backend.result.status==="cancelled"?root.t("أُلغيت العملية؛ النتيجة غير مكتملة","Cancelled; result incomplete"):root.t("تعذرت العملية؛ راجع التفاصيل","Operation failed; see details")}
       RowLayout {visible:backend.result.verifiedBytes!==undefined;Layout.fillWidth:true;Label {text:root.t("المساحة المختبرة: ","Tested: ")+root.size(backend.result.writtenBytes||0);color:"#c2d6e5";Layout.fillWidth:true}Label {text:root.t("اجتازت التحقق: ","Verified: ")+root.size(backend.result.verifiedBytes||0);color:"#8edbc1";Layout.fillWidth:true}Label {text:root.t("فشلت: ","Failed: ")+root.size(backend.result.failedBytes||0);color:"#e0bc79";Layout.fillWidth:true}}
       Label {visible:backend.result.operation==="directory_capacity_test";Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#d5bb7b";text:root.t("هذه ليست السعة الأصلية الكاملة؛ النتيجة تخص ملفات الاختبار فقط.","This is not the full original capacity; results cover only the test files.")}
       TextArea {Layout.fillWidth:true;Layout.preferredHeight:210;text:backend.report;readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;color:"#b9d2e5";font.family:"DejaVu Sans Mono";font.pixelSize:12;LayoutMirroring.enabled:false;background:Rectangle {color:"#08131d";radius:10} }
      }
     }
     Panel {
      visible:root.page===6;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       Label {text:root.t("آخر 50 عملية · سجل محلي","Last 50 operations · local history");color:"#eaf2f9";font.bold:true;font.pixelSize:18}
       Label {visible:backend.storageNotice.length>0;text:backend.storageNotice;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#e5c583"}
       Repeater {model:backend.history;delegate:ActionButton {required property var modelData;Layout.fillWidth:true;text:modelData.created+" · "+modelData.operation;onClicked:root.historyReport=modelData.report} }
       TextArea {visible:root.historyReport.length>0;text:root.historyReport;Layout.fillWidth:true;Layout.preferredHeight:250;readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;color:"#b9d2e5";font.pixelSize:12;LayoutMirroring.enabled:false}
      }
     }
     Label {visible:root.notice.length>0;text:root.notice;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#9fe2c8"}
    }
   }
  }
 }
 onClosing: function(close) {if(backend.busy){backend.cancel();root.notice=root.t("انتظر انتهاء الإلغاء ثم أغلق البرنامج","Wait for cancellation to finish, then close");close.accepted=false}}
}
