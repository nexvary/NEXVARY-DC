import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ApplicationWindow {
 id: root
 width:1280;height:850;minimumWidth:980;minimumHeight:700;visible:true
 title:"NEXVARY Disk Care · 0.6.0";color:"#080f17"
 property bool arabic:true
 property int page:0
 property bool bootRepairMode:false
 property int diskIndex:-1
 property var selectedDisk: diskIndex>=0&&diskIndex<backend.disks.length?backend.disks[diskIndex]:({})
 property string notice:""
 property string historyReport:""
 property bool rescueMode:false
 property bool diskRescueWorkspace:false
 property alias selectiveRetryEnabled:selectiveRetry.checked
 property var isoCallback:null
 property var confirmPlan:({})
 property string confirmText:""
 property bool confirmAck:false
 property alias confirmation:confirmDialog
 property alias exportDialog:reportDialog
 property var names:[t("مركز التخزين","Storage center"),t("التشخيص وSMART","Diagnostics & SMART"),t("إنقاذ البيانات","Data rescue"),t("الصيانة والإصلاح","Maintenance & repair"),t("الفلاشات والكروت","Flash & cards"),t("اختبار السعة","Capacity test"),t("التقارير والسجل","Reports & history"),t("الفورمات والتقسيم","Format & partitions"),t("الإقلاع والتثبيت","Boot & installation"),t("حول المطور","About developer")]
 property var iconNames:["overview","disk","rescue","maintenance","usb","capacity","reports","partition","boot","developer"]
 function t(ar,en){return arabic?ar:en}
 function size(bytes){return (Number(bytes||0)/1073741824).toFixed(2)+" GiB"}
 function operationName(value){const m={boot_repair:t("إصلاح GRUB","GRUB repair"),boot_repair_plan:t("خطة إصلاح الإقلاع","Boot repair plan"),disk_discovery:t("اكتشاف الأقراص","Disk discovery"),smart_read:t("قراءة صحة القرص","Disk health read"),image_read:t("فحص الصورة","Image inspection"),image_copy:t("نسخ الصورة والتحقق","Image copy & verification"),surface_read:t("فحص قراءة السطح","Surface read scan"),media_retry:t("إعادة إنقاذ المناطق المتعثرة","Selective rescue retry"),media_rescue:t("إنشاء صورة إنقاذ","Media rescue image"),directory_capacity_test:t("اختبار كتابة وقراءة المساحة","Storage write/read test"),format:t("فورمات القسم","Partition format"),delete:t("حذف القسم","Partition deletion"),create:t("إنشاء قسم","Partition creation"),layout:t("إنشاء تقسيم جديد","New partition layout"),repair:t("إصلاح نظام الملفات","Filesystem repair"),check:t("فحص نظام الملفات","Filesystem scan"),filesystem_recovery:root.t("استعادة بيانات نظام الملفات","Filesystem metadata recovery"),fat32_recovery:t("استعادة ملفات FAT32 المحذوفة","FAT32 deleted-file recovery"),file_recovery:t("استعادة الصور من صورة الإنقاذ","Recover photos from rescue image"),linux_usb:t("تجهيز إقلاع Linux","Linux boot media"),windows_bios_usb:t("تجهيز Windows BIOS/UEFI","Windows BIOS/UEFI media"),windows_usb:t("تجهيز مثبت Windows","Windows installation media")};return m[value]||value||"—"}
 function message(s){
  if(!arabic)return s
  if(s.indexOf("Administrator")>=0)return "تحتاج العملية صلاحيات المسؤول. استخدم «تشغيل كمسؤول» ثم اختر القرص وأكد العملية من جديد."
  if(s.indexOf("Only external")>=0||s.indexOf("Internal disks")>=0)return "العمليات الكتابية مسموحة لوسائط USB / SD / MMC الخارجية فقط. الأقراص الداخلية محمية."
  if(s.indexOf("Confirmation")>=0)return "التأكيد غير صالح أو انتهت مدته. لم يبدأ أي تغيير؛ راجع العملية وأكدها من جديد."
  if(s.indexOf("identity changed")>=0||s.indexOf("Partition changed")>=0)return "تغيرت هوية القرص أو تقسيمه. حدّث القائمة وراجع الهدف قبل التأكيد."
  if(s.indexOf("smartctl is not")>=0)return "محرك SMART غير متاح؛ تعذر تحديد صحة القرص."
  if(s.indexOf("Storage command completed")>=0)return "اكتمل أمر الصيانة. حدّث الأقراص لعرض التقسيم الجديد."
  return s
 }
 function previewConfirmation(){confirmPlan={action:"layout",model:"TEST FIXTURE — NO STORAGE EXECUTION",device:"REVIEW-ONLY",bytes:8589934592,challenge:"REVIEW-ONLY",token:"",allData:true};confirmDialog.open()}
 function chooseIso(callback){isoCallback=callback;isoDialog.open()}
 LayoutMirroring.enabled:arabic;LayoutMirroring.childrenInherit:true
 font.family:Qt.platform.os==="windows"?"Segoe UI":"DejaVu Sans";font.pixelSize:13
 palette.placeholderText:"#89a9c0";palette.text:"#dce9f4";palette.windowText:"#dce9f4";palette.base:"#0c1b28";palette.button:"#19344a";palette.buttonText:"#dce9f4";palette.highlight:"#3985bd";palette.highlightedText:"#ffffff"
 Connections {target:backend;function onDisksChanged(){if(root.diskIndex>=backend.disks.length)root.diskIndex=-1}}
 FileDialog {id:sourceDialog;onAccepted:imagePath.text=backend.localPath(selectedFile.toString())}
 FileDialog {id:priorRescueDialog;onAccepted:priorRescuePath.text=backend.localPath(selectedFile.toString())}
 FileDialog {id:rescueDialog;fileMode:FileDialog.SaveFile;onAccepted:rescuePath.text=backend.localPath(selectedFile.toString())}
 FileDialog {id:targetDialog;fileMode:FileDialog.SaveFile;onAccepted:targetPath.text=backend.localPath(selectedFile.toString())}
 FileDialog {id:isoDialog;nameFilters:["ISO (*.iso)"];onAccepted:if(root.isoCallback)root.isoCallback(backend.localPath(selectedFile.toString()))}
 FileDialog {id:reportDialog;fileMode:FileDialog.SaveFile;nameFilters:["JSON (*.json)"];onAccepted:root.notice=backend.exportReport(backend.localPath(selectedFile.toString()))?root.t("تم حفظ التقرير","Report saved"):root.t("تعذر الحفظ؛ اختر اسم ملف جديدًا","Save failed; choose a new filename")}
 FolderDialog {id:recoveryFolder;onAccepted:recoveryPath.text=backend.localPath(selectedFolder.toString())}
 FolderDialog {id:folderDialog;onAccepted:capacityPath.text=backend.localPath(selectedFolder.toString())}
 Dialog {
  id:confirmDialog;modal:true;anchors.centerIn:parent;width:Math.min(root.width-80,640);closePolicy:Popup.CloseOnEscape
  title:root.t("تأكيد إجراء على وسيط التخزين","Confirm storage operation")
  onOpened:{confirmInput.text="";confirmBackup.checked=false}
  background:Rectangle {color:"#142333";border.color:"#ca8358";radius:12}
  contentItem:ColumnLayout {spacing:14
   RowLayout {AppIcon {name:"maintenance";Layout.preferredWidth:48;Layout.preferredHeight:48}Label {text:root.operationName(root.confirmPlan.action);font.bold:true;font.pixelSize:22;color:"#ffc287";Layout.fillWidth:true;wrapMode:Text.WordWrap} }
   Label {text:(root.confirmPlan.model||"")+"\n"+(root.confirmPlan.device||"")+" · "+root.size(root.confirmPlan.bytes)+(root.confirmPlan.partition?"\n"+root.t("القسم رقم ","Partition #")+root.confirmPlan.partition:"");Layout.fillWidth:true;wrapMode:Text.WrapAnywhere;color:"#edf5fc";font.bold:true;LayoutMirroring.enabled:false}
   Label {text:root.confirmPlan.action==="check"?root.t("فحص نظام الملفات على القسم المحدد. قد يحتاج Windows إلى وصول حصري.","Scan the selected partition filesystem. Windows may require exclusive access."):root.confirmPlan.allData?root.t("سيُحذف تقسيم القرص وكل البيانات عليه. لا يمكن التراجع من داخل البرنامج.","The entire disk layout and all its data will be erased. This cannot be undone in the application."):root.confirmPlan.action==="repair"?root.t("سيعدل الإصلاح نظام الملفات وقد يعزل أو يحذف بيانات تالفة. احتفظ بنسخة قبل المتابعة.","Repair modifies the filesystem and may isolate or discard damaged data. Back up first."):root.t("سيتم تعديل القسم المحدد. الفورمات أو الحذف يؤديان إلى فقد ملفاته.","The selected partition will be changed. Formatting or deletion loses its files.");color:"#ffc89d";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   Label {text:root.t("للتأكيد، اكتب مسار الجهاز كما يظهر أعلاه:","To confirm, type the device path shown above:");color:"#b6cada";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   TextField {id:confirmInput;Layout.fillWidth:true;selectByMouse:true;LayoutMirroring.enabled:false;onTextChanged:root.confirmText=text}
   CheckBox {id:confirmBackup;Layout.fillWidth:true;text:root.t("راجعت الهدف واحتفظت بنسخة وأوافق على الإجراء","I reviewed the target, backed up and accept the operation");onToggled:root.confirmAck=checked}
   RowLayout {Layout.fillWidth:true;ActionButton {text:root.t("لا، إلغاء","No, cancel");primary:true;focus:true;onClicked:confirmDialog.close()}Item {Layout.fillWidth:true}ActionButton {text:root.t("نعم، نفّذ الإجراء","Yes, execute");danger:true;enabled:root.confirmAck&&root.confirmText===root.confirmPlan.challenge;onClicked:{confirmDialog.close();backend.executeStorage(root.confirmPlan.token,root.confirmText,root.confirmAck)}}}
  }
 }
 Dialog {id:capacityConfirm;modal:true;anchors.centerIn:parent;width:Math.min(root.width-80,580);title:root.t("تأكيد اختبار الكتابة","Confirm write test");standardButtons:Dialog.Yes|Dialog.No
  contentItem:Label {text:root.t("أنت على وشك كتابة ملفات اختبار في:\n","You are about to write test files in:\n")+capacityPath.text+"\n"+testSize.value+" MiB\n"+root.t("على وسيط مغشوش قد تتلف ملفات موجودة رغم استخدام المساحة الفارغة. هل توافق؟","Counterfeit storage may corrupt existing files even when writing free space. Proceed?");wrapMode:Text.WrapAnywhere;color:"#f2c389"}
  onAccepted:backend.testCapacity(capacityPath.text,testSize.value,ack.checked)
 }
 Dialog {id:readConfirm;modal:true;anchors.centerIn:parent;width:Math.min(root.width-80,580);title:root.t("مراجعة قراءة القرص","Review disk read");standardButtons:Dialog.Yes|Dialog.No
  contentItem:Label {text:(root.selectedDisk.model||"")+"\n"+(root.selectedDisk.device||"")+" · "+root.size(root.selectedDisk.bytes)+"\n"+root.t("قراءة كاملة للوسيط دون الكتابة عليه. قد تجهد وسيطًا متعثرًا. الإنقاذ يكتب الصورة في قرص آخر ويملأ المقاطع غير المقروءة بأصفار مع تسجيلها. هل توافق؟","Reads the whole device without writing to it. This can stress failing media. Rescue writes to another disk and records unreadable zero-filled chunks. Proceed?");color:"#edc18a";wrapMode:Text.WrapAnywhere}
  onAccepted:if(root.rescueMode&&selectiveRetry.checked)backend.retryRescueDisk(root.selectedDisk.device,priorRescuePath.text,rescuePath.text,true,resumeRescue.checked,sectorChoice.currentIndex===0?512:4096,retryCount.value);else if(root.rescueMode)backend.rescueDisk(root.selectedDisk.device,rescuePath.text,true,resumeRescue.checked,sectorChoice.currentIndex===0?512:4096,retryCount.value);else backend.scanSurface(root.selectedDisk.device,true)
 }
 RowLayout {anchors.fill:parent;spacing:0
  Rectangle {Layout.preferredWidth:210;Layout.fillHeight:true;color:"#0d1c29";border.color:"#263e50"
   ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:6
    RowLayout {spacing:9;AppIcon {name:"brand";Layout.preferredWidth:46;Layout.preferredHeight:46}ColumnLayout {spacing:0;Label {text:"NEXVARY";font.pixelSize:20;font.bold:true;color:"#eff5fb"}Label {text:"DISK CARE";font.pixelSize:10;font.letterSpacing:2;color:"#7cc2ee"}}}
    Rectangle {Layout.fillWidth:true;height:1;color:"#2e465a";Layout.topMargin:12;Layout.bottomMargin:8}
    Repeater {model:root.names;delegate:Button {required property string modelData;required property int index;Layout.fillWidth:true;implicitHeight:45
     onClicked:{root.page=index;root.notice=""}
     background:Rectangle {radius:8;color:root.page===index?"#1b3c54":parent.hovered?"#162b3c":"transparent";border.color:root.page===index?"#3e7294":"transparent"}
     contentItem:RowLayout {spacing:10;AppIcon {name:root.iconNames[index];Layout.preferredWidth:30;Layout.preferredHeight:30}Label {text:modelData;color:root.page===index?"#f3f9ff":"#adc1d0";font.bold:root.page===index;Layout.fillWidth:true;font.pixelSize:12;wrapMode:Text.WordWrap}}
    }}
    Item {Layout.fillHeight:true}
    ActionButton {visible:backend.windows&&!backend.administrator;Layout.fillWidth:true;implicitHeight:34;text:root.t("تشغيل كمسؤول","Run as administrator");enabled:!backend.busy;onClicked:if(!backend.relaunchAdministrator())root.notice=root.t("لم تُمنح صلاحيات المسؤول؛ لم يبدأ أي إجراء","Administrator permission was not granted; no operation started")}
    Label {visible:!backend.windows||backend.administrator;text:backend.administrator?root.t("صلاحيات مسؤول","Administrator"):root.t("وضع المستخدم","User mode");color:backend.administrator?"#efbf78":"#74c7a7";font.pixelSize:11}
    RowLayout {Label {text:"0.6.0";color:"#7999b0"}Item {Layout.fillWidth:true}ActionButton {text:root.arabic?"English":"العربية";implicitWidth:102;implicitHeight:34;onClicked:root.arabic=!root.arabic}}
   }
  }
  ColumnLayout {Layout.fillWidth:true;Layout.fillHeight:true;Layout.margins:20;spacing:12
   RowLayout {Layout.fillWidth:true;AppIcon {name:root.iconNames[root.page];Layout.preferredWidth:42;Layout.preferredHeight:42}
    ColumnLayout {Layout.fillWidth:true;spacing:3;Label {text:root.names[root.page];font.pixelSize:24;font.bold:true;color:"#edf5fc"}Label {text:root.t("اختر الوسيط • افحص الحالة • راجع الإجراء","Select media • inspect health • review the action");color:"#8ba9c0";font.pixelSize:12}}
    ActionButton {visible:root.page!==0;text:root.t("رجوع","Back");implicitWidth:85;onClicked:root.page=0}
    ActionButton {text:root.t("تحديث","Refresh");implicitWidth:95;enabled:!backend.busy;onClicked:backend.refresh()}
   }
   Rectangle {Layout.fillWidth:true;implicitHeight:68;color:"#112333";radius:9;border.color:"#2b4a61"
    RowLayout {anchors.fill:parent;anchors.margins:12;spacing:12;AppIcon {name:"disk";Layout.preferredWidth:36;Layout.preferredHeight:36}
     ComboBox {id:diskSelector;Layout.fillWidth:true;model:backend.disks.map(d=>(d.model||d.device)+" · "+root.size(d.bytes)+" · "+d.device);currentIndex:root.diskIndex;displayText:root.diskIndex<0?root.t("اختر القرص المستهدف","Select target disk"):currentText;enabled:!backend.busy;onActivated:root.diskIndex=currentIndex}
     Label {text:root.selectedDisk.external?root.t("خارجي","External"):root.selectedDisk.device?root.t("داخلي / محمي","Internal / protected"):"—";color:root.selectedDisk.external?"#72d5ab":"#efc07f"}
    }
   }
   Rectangle {visible:backend.busy||root.notice.length>0;Layout.fillWidth:true;implicitHeight:52;color:"#182737";radius:8
    RowLayout {anchors.fill:parent;anchors.margins:10;Label {text:backend.busy?(backend.interruptible?root.t("جارٍ التنفيذ…","Working…"):root.t("جارٍ تغيير الوسيط — لا تفصله أو تغلق البرنامج","Changing media — do not disconnect or close")):root.message(root.notice);Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#eac184"}ProgressBar {visible:backend.busy;Layout.preferredWidth:120;value:backend.progress;indeterminate:!backend.interruptible||backend.result.operation==="disk_discovery"||backend.result.operation==="smart_read"}ActionButton {visible:backend.busy&&backend.interruptible;text:root.t("إلغاء","Cancel");implicitWidth:85;onClicked:backend.cancel()}}
   }
   ScrollView {id:scroll;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
    ColumnLayout {width:scroll.availableWidth;spacing:12
     RowLayout {visible:root.page===0;Layout.fillWidth:true;spacing:10
      Repeater {model:[{n:backend.disks.length,title:root.t("أقراص فعلية","Physical disks"),icon:"disk"},{n:backend.volumes.length,title:root.t("وحدات مركبة","Mounted volumes"),icon:"partition"},{n:backend.history.length,title:root.t("تقارير محفوظة","Saved reports"),icon:"reports"}];delegate:Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:78;radius:9;color:"#112131";border.color:"#2b4357";RowLayout {anchors.fill:parent;anchors.margins:12;AppIcon {name:modelData.icon;Layout.preferredWidth:38;Layout.preferredHeight:38}ColumnLayout {Layout.fillWidth:true;Label {text:modelData.n;color:"#e6f3ff";font.pixelSize:23;font.bold:true}Label {text:modelData.title;color:"#92b0c7";font.pixelSize:11}}}}}
     }
     Panel {visible:root.page===0||root.page===1;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:10
       RowLayout {Label {text:root.t("الأقراص المتصلة","Connected disks");font.pixelSize:18;font.bold:true;color:"#e4f1fc";Layout.fillWidth:true}Label {text:root.t("حدد قرصًا لعرض بياناته","Select a disk to inspect");color:"#829fb7";font.pixelSize:11}}
       Label {visible:backend.disks.length===0;text:root.t("لم تظهر أقراص. راجع نتيجة الاكتشاف أو حدّث القائمة.","No disks found. Check discovery results or refresh.");color:"#f2bd75";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       Repeater {model:backend.disks;delegate:Rectangle {required property var modelData;required property int index;Layout.fillWidth:true;implicitHeight:86;radius:8;color:root.diskIndex===index?"#18354b":"#0b1925";border.color:root.diskIndex===index?"#55a5d4":"#243e52"
        RowLayout {anchors.fill:parent;anchors.margins:12;AppIcon {name:modelData.external?"usb":"disk";Layout.preferredWidth:44;Layout.preferredHeight:44}ColumnLayout {Layout.fillWidth:true;spacing:3;Label {text:modelData.model||modelData.device;font.bold:true;color:"#e0edf7";Layout.fillWidth:true;elide:Text.ElideRight}Label {text:root.size(modelData.bytes)+" · "+(modelData.transport||"—")+" · "+(modelData.style||"");color:"#79bce4"}Label {text:modelData.device+" · "+(modelData.serial||"—");color:"#89a4b9";font.pixelSize:11;Layout.fillWidth:true;elide:Text.ElideRight;LayoutMirroring.enabled:false}}
         ActionButton {text:root.t("اختيار","Select");implicitWidth:85;enabled:!backend.busy;onClicked:root.diskIndex=index}
         ActionButton {text:"SMART";primary:true;implicitWidth:90;enabled:!backend.busy;onClicked:{root.diskIndex=index;root.page=1;backend.inspectHealth(modelData.device)}}
        }
       }}
       Label {text:root.t("SMART يعرض مؤشرات القرص، وليس ضمانًا ضد الأعطال. لا تُفسّر غياب القراءة على أنه سلامة.","SMART reports drive indicators, not a failure guarantee. Missing data does not mean healthy storage.");color:"#88a6bc";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:11}
      }
     }
     GridLayout {visible:root.page===0;Layout.fillWidth:true;columns:2;columnSpacing:10;rowSpacing:10
      Repeater {model:[{p:7,k:"partition",title:root.t("الفورمات وإدارة الأقسام","Format & manage partitions"),desc:root.t("NTFS / exFAT / FAT32 • GPT / MBR","NTFS / exFAT / FAT32 • GPT / MBR")},{p:8,k:"boot",title:root.t("وسيط تثبيت Windows","Windows installation media"),desc:root.t("UEFI x64 • تحقق من الملفات","UEFI x64 • verified file copy")},{p:5,k:"capacity",title:root.t("اختبار الفلاشات والكروت","Test flash drives & cards"),desc:root.t("كتابة ثم قراءة • كشف اختلاف البيانات","Write then read • detect mismatches")},{p:2,k:"rescue",title:root.t("صور الأقراص والإنقاذ","Disk images & rescue"),desc:root.t("فحص • نسخة موثقة • SHA-256","Inspect • verified copy • SHA-256")}];delegate:Button {required property var modelData;Layout.fillWidth:true;implicitHeight:96;onClicked:root.page=modelData.p;background:Rectangle {radius:9;color:parent.hovered?"#19344a":"#102131";border.color:"#314f65"}contentItem:RowLayout {spacing:12;AppIcon {name:modelData.k;Layout.preferredWidth:46;Layout.preferredHeight:46}ColumnLayout {Layout.fillWidth:true;Label {text:modelData.title;font.bold:true;color:"#e6f1fa";Layout.fillWidth:true;wrapMode:Text.WordWrap}Label {text:modelData.desc;color:"#8dadc5";font.pixelSize:11;Layout.fillWidth:true;wrapMode:Text.WordWrap}}}}}
     }
     RowLayout {visible:root.page===2;Layout.fillWidth:true
      ActionButton {text:root.t("استعادة وفحص صورة","Recover & inspect image");primary:!root.diskRescueWorkspace;onClicked:root.diskRescueWorkspace=false}
      ActionButton {text:root.t("إنقاذ قرص وإعادة المحاولة","Image disk & retry");primary:root.diskRescueWorkspace;onClicked:root.diskRescueWorkspace=true}
     }
     Panel {visible:root.page===2&&!root.diskRescueWorkspace;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       Label {text:root.t("استعادة الملفات من صورة الإنقاذ","Recover files from rescue image");font.pixelSize:20;font.bold:true;color:"#e3f1fa"}
       Label {text:root.t("اختر ملف صورة الإنقاذ، وحدد مجلدًا على قرص الاستعادة. البحث بالتواقيع يستخرج الصور المتصلة بمعالجة متدفقة، بما فيها بيانات محذوفة لم تُكتب فوقها. لا يعيد أسماء الملفات أو المجلدات أو الملفات المجزأة. تُفحص CRC لصور PNG؛ صور JPEG تحتاج مراجعة بصرية. يحتفظ بالنتائج عند الإلغاء دون تعديل صورة المصدر.","Select a rescue image and a folder on your recovery disk. Signature mode streams contiguous photos without a 64 MiB file cap, including deleted data that was not overwritten. Original names, folders and fragmented files are not restored. PNG CRC is checked; JPEG candidates need visual review. Cancellation retains results without modifying the source image.");color:"#a5bfd2";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       RowLayout {Layout.fillWidth:true;TextField {id:imagePath;Layout.fillWidth:true;placeholderText:root.t("ملف الصورة المصدر","Source image");selectByMouse:true;enabled:!backend.busy}ActionButton {text:root.t("اختيار","Browse");implicitWidth:100;onClicked:sourceDialog.open()}}
       ComboBox {id:recoveryMode;Layout.fillWidth:true;enabled:!backend.busy;model:[root.t("صور PNG / JPEG — البحث بالتواقيع","PNG / JPEG signature carving"),root.t("ملفات FAT32 المحذوفة","Deleted FAT32 files"),"NTFS","exFAT"]}
       Label {visible:recoveryMode.currentIndex===1;text:root.t("FAT32: صورة قسم أو قرص بنظام GPT أو MBR. يستخرج ملفات محذوفة حين تكون كتلها المتصلة فارغة؛ لا يضمن سلامة المحتوى. الحرف الأول من الاسم القصير مفقود. يدعم GPT وMBR الممتد، والأسماء الطويلة والمجلدات عند بقاء بياناتها. النتائج مرشحة للفحص ولا تضمن سلامة المحتوى.","FAT32: volume image or GPT/MBR disk image. Extracts deleted contiguous files whose clusters are still free; content integrity is not guaranteed. First short-name character is lost. GPT and extended MBR are supported. Long names and folders are retained when metadata survives. Results remain candidates; allocation does not prove original content.");color:"#e7bc7b";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       RowLayout {Layout.fillWidth:true;TextField {id:recoveryPath;Layout.fillWidth:true;placeholderText:root.t("مجلد الاستعادة على قرص آخر","Recovery folder on another disk");selectByMouse:true;enabled:!backend.busy}ActionButton {text:root.t("اختيار مجلد","Choose folder");onClicked:recoveryFolder.open()}}
       RowLayout {Layout.fillWidth:true;Label {text:root.t("أقصى عدد ملفات (أنظمة الملفات):","Maximum files (filesystem recovery):");color:"#c1d5e6";Layout.fillWidth:true;wrapMode:Text.WordWrap}SpinBox {id:fileLimit;from:1;to:1000000;value:100000;stepSize:10000;editable:true;enabled:!backend.busy}}
       Label {visible:recoveryMode.currentIndex>=2;text:root.t("استعادة من بيانات النظام: أسماء ومجلدات ومحتوى مجزأ عند بقاء التخصيص. كامل يعني اكتمال عدد البايتات، وليس ضمان سلامة المحتوى. لا يدعم NTFS المضغوط أو المشفر أو امتدادات ATTRIBUTE_LIST الخارجية لجدول MFT؛ تظهر القيود في التقرير.","Metadata recovery: names, folders and fragmented content when allocation survives. Complete means byte coverage, not content integrity. Compressed/encrypted NTFS and external MFT ATTRIBUTE_LIST extensions are reported as unsupported.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#e7bc7b"}
       ActionButton {text:root.t("بدء استعادة الملفات","Start file recovery");primary:true;enabled:!backend.busy&&imagePath.text.length>0&&recoveryPath.text.length>0;onClicked:backend.recoverImage(imagePath.text,recoveryPath.text,recoveryMode.currentIndex,fileLimit.value)}
      }
     }
     Panel {visible:root.page===1||(root.page===2&&root.diskRescueWorkspace);Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       Label {text:root.page===1?root.t("فحص السطح بالقراءة فقط","Read-only surface scan"):root.t("إنشاء صورة إنقاذ للقرص","Create rescue disk image");font.pixelSize:19;font.bold:true;color:"#dbeefc"}
       Label {text:root.t("يتطلب اختيار قرص فعلي. الفحص لا يصلح القطاعات؛ فحص السطح يسجل كتل 1 MiB؛ الإنقاذ يعيد المحاولة بدقة القطاع المختار. احفظ الصورة وخريطة JSONL معًا. الاستئناف يتحقق من هوية المصدر والوجهة وSHA-256 قبل المتابعة.","Select a physical disk. Scanning does not repair sectors; surface scan reports 1 MiB blocks; rescue retries at the selected sector size. Keep the image and JSONL map together. Explicit resume verifies source/destination identity and SHA-256 before continuing.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#a6bfd1"}
       Flow {visible:root.page===2;Layout.fillWidth:true;spacing:10;CheckBox {id:resumeRescue;text:root.t("استئناف صورة موجودة","Resume existing image");enabled:!backend.busy}ComboBox {id:sectorChoice;model:["512 bytes","4096 bytes"];enabled:!backend.busy}Label {text:root.t("إعادة المحاولة لكل قطاع:","Retries per sector:");color:"#c1d5e6"}SpinBox {id:retryCount;from:0;to:5;value:1;enabled:!backend.busy}}
       CheckBox {id:selectiveRetry;visible:root.page===2;text:root.t("إعادة قراءة المناطق غير المقروءة فقط من صورة سابقة","Retry only unreadable regions from a previous image");enabled:!backend.busy}
       Label {visible:root.page===2&&selectiveRetry.checked;text:root.t("يتحقق من صورة مكتملة وخريطتها أولًا، وينسخ البيانات المقروءة إلى صورة أخرى دون إعادة قراءتها من الهارد. يحتاج مساحة لصورة ثانية؛ يحافظ على الأصل. لا يضبط مهلة عتاد ولا يفصل الطاقة.","Validates a full image/map first, copies healthy image data to a separate image and reads only recorded failed sectors from the drive. Requires room for another full image; preserves the original. No hardware timeout or power control.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#a6bfd1"}
       RowLayout {visible:root.page===2&&selectiveRetry.checked;Layout.fillWidth:true;TextField {id:priorRescuePath;Layout.fillWidth:true;placeholderText:root.t("صورة الإنقاذ السابقة مع خريطة JSONL الأصلية","Previous rescue image with its original JSONL map");enabled:!backend.busy}ActionButton {text:root.t("الصورة السابقة","Previous image");onClicked:priorRescueDialog.open()}}
       RowLayout {visible:root.page===2;Layout.fillWidth:true;TextField {id:rescuePath;Layout.fillWidth:true;placeholderText:root.t("صورة جديدة على قرص آخر","New image on a different disk");selectByMouse:true;enabled:!backend.busy}ActionButton {text:root.t("الوجهة","Destination");onClicked:rescueDialog.open()}}
       ActionButton {text:root.page===1?root.t("مراجعة فحص السطح","Review surface scan"):root.t("مراجعة إنشاء صورة الإنقاذ","Review rescue imaging");primary:true;enabled:!backend.busy&&!!root.selectedDisk.device&&(root.page===1||(rescuePath.text.length>0&&(!selectiveRetry.checked||priorRescuePath.text.length>0)));onClicked:{root.rescueMode=root.page===2;readConfirm.open()}}
       Label {visible:!backend.windows&&root.page===2;text:root.t("إنشاء صور الأقراص الفعلية متاح على Windows حاليًا؛ على Linux يمكنك فحص السطح وملفات الصور.","Physical-disk imaging currently supports Windows; Linux supports surface and image-file scanning.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#efbd77"}
      }
     }
     Panel {visible:root.page===2&&!root.diskRescueWorkspace;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       Label {text:root.t("فحص ونسخ صورة القرص","Inspect and copy disk image");font.pixelSize:20;font.bold:true;color:"#e3f1fa"}
       Label {text:root.t("اختر ملف صورة عاديًا. لن يُستبدل أي ملف موجود. إنقاذ الوسيط المتعثر واستعادة الملفات المحذوفة مساران منفصلان عن النسخ العادي.","Select a regular image file. Existing files are never overwritten. Failing-media rescue and deleted-file recovery are separate from regular copying.");color:"#9ebbd0";Layout.fillWidth:true;wrapMode:Text.WordWrap}

       RowLayout {Layout.fillWidth:true;TextField {id:targetPath;Layout.fillWidth:true;placeholderText:root.t("ملف جديد للنسخة","New copy file");selectByMouse:true;enabled:!backend.busy}ActionButton {text:root.t("الوجهة","Destination");implicitWidth:100;onClicked:targetDialog.open()}}
       Flow {Layout.fillWidth:true;spacing:10;ActionButton {text:root.t("فحص الصورة","Inspect image");primary:true;enabled:!backend.busy&&imagePath.text.length>0;onClicked:backend.scanImage(imagePath.text)}ActionButton {text:root.t("نسخ والتحقق","Copy & verify");enabled:!backend.busy&&imagePath.text.length>0&&targetPath.text.length>0;onClicked:backend.copyImage(imagePath.text,targetPath.text)}}
      }
     }
     RowLayout {visible:root.page===8;Layout.fillWidth:true
      ActionButton {text:root.t("تجهيز فلاشة تثبيت","Create installation media");primary:!root.bootRepairMode;onClicked:root.bootRepairMode=false}
      ActionButton {text:root.t("إصلاح الإقلاع","Repair boot");primary:root.bootRepairMode;onClicked:root.bootRepairMode=true}
     }
     StorageWorkspace {app:root;visible:root.page===3||root.page===4||root.page===7||(root.page===8&&!root.bootRepairMode);bootMode:root.page===8;Layout.fillWidth:true}
     BootRepairWorkspace {app:root;visible:root.page===8&&root.bootRepairMode;Layout.fillWidth:true}
     Panel {visible:root.page===3;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:10;Label {text:root.t("Firmware ومعالجة التلف المادي","Firmware and physical damage");font.bold:true;font.pixelSize:18;color:"#e6c382"}Label {text:root.t("نسخة Firmware تظهر في نتيجة SMART عند دعم الجهاز. تحديثه يحتاج حزمة متوافقة مع الموديل من الشركة المصنّعة؛ لا يوجد تحديث عام لكل الأقراص. إصلاح نظام الملفات لا يعالج تلف السطح المادي.","Firmware version is shown in SMART when supported. Updating requires a model-matched manufacturer package; no universal drive update exists. Filesystem repair does not repair physical surface damage.");color:"#b6cbdb";Layout.fillWidth:true;wrapMode:Text.WordWrap}ActionButton {text:root.t("قراءة بيانات الجهاز","Read drive details");enabled:!backend.busy&&!!root.selectedDisk.device;onClicked:backend.inspectHealth(root.selectedDisk.device)}}
     }
     Panel {visible:root.page===5;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:12
       RowLayout {AppIcon {name:"capacity";Layout.preferredWidth:44;Layout.preferredHeight:44}Label {text:root.t("اختبار موثوقية المساحة المكتوبة","Verify written storage space");font.pixelSize:20;font.bold:true;color:"#e7f2fc";Layout.fillWidth:true;wrapMode:Text.WordWrap}}
       Label {text:root.t("تُكتب جميع ملفات الاختبار أولًا، ثم تُقرأ للكشف عن تكرار العناوين أو تلف البيانات. النتيجة تخص المساحة المختارة فقط؛ السعة الأصلية الكاملة تحتاج فحصًا شاملًا.","All test files are written first, then read to detect address wraparound or corruption. Results cover selected space only; full physical capacity requires a comprehensive probe.");color:"#a5bfd2";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       ComboBox {Layout.fillWidth:true;model:backend.volumes.map(v=>v.root+" · "+root.size(v.free)+" "+root.t("متاح","free"));onActivated:capacityPath.text=backend.volumes[currentIndex].root;enabled:!backend.busy}
       RowLayout {Layout.fillWidth:true;TextField {id:capacityPath;Layout.fillWidth:true;placeholderText:root.t("مجلد على الوسيط المستهدف","Folder on target media");selectByMouse:true;enabled:!backend.busy}ActionButton {text:root.t("اختر مجلدًا","Choose folder");onClicked:folderDialog.open()}}
       RowLayout {Label {text:root.t("مساحة الاختبار (MiB)","Test size (MiB)");color:"#c1d4e4"}SpinBox {id:testSize;from:1;to:1048576;value:64;editable:true;enabled:!backend.busy}Item {Layout.fillWidth:true}Label {text:root.t("احتياطي 64 MiB","64 MiB reserve");color:"#89a9c0"}}
       Label {text:root.t("انسخ بياناتك قبل الاختبار. الكتابة على وسيط مغشوش قد تتلف ملفات موجودة. تُحذف ملفات اختبار البرنامج بعد العملية.","Back up before testing. Writing counterfeit media can corrupt existing files. Application test files are removed afterward.");color:"#f2c07d";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       CheckBox {id:ack;text:root.t("احتفظت بنسخة خارج الوسيط المستهدف","I backed up outside the target media");enabled:!backend.busy}
       ActionButton {text:root.t("بدء اختبار الكتابة والقراءة","Start write/read test");primary:true;enabled:!backend.busy&&ack.checked&&capacityPath.text.length>0;onClicked:capacityConfirm.open()}
      }
     }
     ResultPanel {app:root;visible:root.page!==0&&root.page!==9&&backend.report.length>0;Layout.fillWidth:true}
     Panel {visible:root.page===6;Layout.fillWidth:true
      ColumnLayout {anchors.fill:parent;spacing:10;Label {text:root.t("آخر 50 عملية","Last 50 operations");font.pixelSize:18;font.bold:true;color:"#dcecf8"}Label {visible:backend.storageNotice.length>0;text:backend.storageNotice;color:"#f2c07d";Layout.fillWidth:true;wrapMode:Text.WordWrap}
       Repeater {model:backend.history;delegate:ActionButton {required property var modelData;Layout.fillWidth:true;text:modelData.created+" · "+root.operationName(modelData.operation);onClicked:root.historyReport=modelData.report}}
       TextArea {visible:root.historyReport.length>0;text:root.historyReport;readOnly:true;selectByMouse:true;Layout.fillWidth:true;Layout.preferredHeight:200;wrapMode:TextEdit.WrapAnywhere;LayoutMirroring.enabled:false}
      }
     }
     AboutWorkspace {app:root;visible:root.page===9;Layout.fillWidth:true}
    }
   }
  }
 }
 onClosing:function(close){if(backend.busy){if(backend.interruptible)backend.cancel();root.notice=root.t("انتظر انتهاء العملية قبل الإغلاق","Wait for the operation before closing");close.accepted=false}}
}
