import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Panel {
 required property var app
 ColumnLayout {
  anchors.fill:parent;spacing:12
  Label {text:app.t("إصلاح إقلاع Linux بعد تثبيت Windows","Repair Linux boot after installing Windows");font.pixelSize:20;font.bold:true;color:"#eff5fa";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {visible:!backend.windows;text:app.t("يعمل من Linux حي على x64، باستخدام أدوات GRUB المثبتة في البيئة الحية. اختر قسم Linux المركّب وقسم EFI والقرص بدقة. تُحفظ ملفات الإقلاع قبل الكتابة. لا يُحذف أي قسم.","Run from x64 live Linux with GRUB tools installed. Select the mounted Linux partition, ESP and whole disk carefully. Boot files are backed up before writes. No partitions are deleted.");color:"#b2c5d5";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {visible:!backend.windows;text:app.t("الدعم: جذر ext4 عادي و/boot داخله؛ UEFI دون Secure Boot، أو BIOS على MBR. التشفير وLVM و/boot المنفصل غير مدعومة. تحتاج إعادة التشغيل للتحقق من نظاميك.","Supports plain ext4 root with /boot inside it; UEFI without Secure Boot, or BIOS on MBR. Encryption, LVM and separate /boot are unsupported. Reboot to verify both installed systems.");color:"#efbd77";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {visible:backend.windows;text:app.t("لإصلاح GRUB، أقلع من Linux حي وشغّل نسخة Linux أو الأداة المرفقة. تجهيز فلاشة التثبيت بالأعلى لا يصلح إقلاع القرص الداخلي.","To repair GRUB, boot live Linux and run the Linux build or bundled helper. Creating installation media above does not repair internal-disk boot.");color:"#efbd77";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  ColumnLayout {visible:backend.windows;Layout.fillWidth:true;spacing:10
   Label {text:app.t("استعادة أولوية Linux في UEFI من Windows","Restore Linux UEFI priority from Windows");color:"#eff5fa";font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap}
   Label {text:app.t("إذا بقي إدخال GRUB أو shim في الفيرموير، يمكن تقديمه على Windows Boot Manager دون كتابة ملفات الأقسام. هذا لا يعيد إنشاء ملفات EFI المفقودة. يحتاج المسؤول؛ ويُرفض التنفيذ إذا تعذر تفسير ترتيب الإقلاع بأمان.","If a GRUB or shim firmware entry survives, it can be moved before Windows Boot Manager without writing partition files. This does not recreate missing EFI files. Requires administrator permission and a safely parseable firmware order.");color:"#b2c5d5";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   ActionButton {text:app.t("فحص إدخالات إقلاع Linux","Inspect Linux firmware entries");enabled:!backend.busy&&backend.administrator;onClicked:backend.prepareFirmwareBoot()}
   ComboBox {id:firmwareEntries;Layout.fillWidth:true;property var entries:backend.result.entries||[];model:entries.map(e=>e.id+" · "+e.path);enabled:!backend.busy}
   ActionButton {text:app.t("عرض خطة تغيير الأولوية","Review priority change");enabled:!backend.busy&&firmwareEntries.currentIndex>=0;onClicked:backend.prepareFirmwareBoot(firmwareEntries.entries[firmwareEntries.currentIndex].id)}
  }
  ComboBox {id:mode;visible:!backend.windows;model:["UEFI","BIOS / MBR"];enabled:!backend.busy&&!backend.windows;Layout.fillWidth:true}
  TextField {id:linuxRoot;visible:!backend.windows;placeholderText:app.t("مسار تركيب قسم Linux، مثل /mnt/linux","Linux root mount, e.g. /mnt/linux");Layout.fillWidth:true;enabled:!backend.busy&&!backend.windows;selectByMouse:true}
  TextField {id:esp;visible:!backend.windows&&mode.currentIndex===0;placeholderText:app.t("مسار تركيب EFI، مثل /mnt/esp","ESP mount, e.g. /mnt/esp");Layout.fillWidth:true;enabled:!backend.busy&&!backend.windows;selectByMouse:true}
  TextField {id:disk;visible:!backend.windows;placeholderText:app.t("القرص الكامل، مثل /dev/nvme0n1","Whole disk, e.g. /dev/nvme0n1");Layout.fillWidth:true;enabled:!backend.busy&&!backend.windows;selectByMouse:true}
  TextField {id:backup;visible:!backend.windows;placeholderText:app.t("مجلد نسخ احتياطي على وسيط آخر","Backup directory on another device");Layout.fillWidth:true;enabled:!backend.busy&&!backend.windows;selectByMouse:true}
  ActionButton {visible:!backend.windows;text:app.t("فحص وعرض خطة الإصلاح","Inspect and show repair plan");enabled:!backend.busy&&!backend.windows;onClicked:backend.prepareBootRepair(linuxRoot.text,esp.text,disk.text,mode.currentIndex===0?"uefi":"bios",backup.text)}
  Label {visible:!!backend.result.plan;text:backend.windows?app.t("راجع الخطة واكتب تأكيد BOOT ومعرّف الإدخال كما يظهر أدناه.","Review the plan and type BOOT plus the entry identifier exactly as shown below."):app.t("راجع التقرير أدناه، ثم اكتب REPAIR ومسار القرص كما ورد في الخطة. التنفيذ يحتاج root من البيئة الحية.","Review the report below, then type REPAIR and the exact disk path from the plan. Execution requires root in live Linux.");color:"#ffcd9e";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  TextField {id:confirmation;visible:!!backend.result.plan;placeholderText:backend.result.confirmation||"REPAIR /dev/…";Layout.fillWidth:true;enabled:!backend.busy;selectByMouse:true}
  ActionButton {visible:!!backend.result.plan;text:backend.windows?app.t("حفظ النسخة وتغيير أولوية الإقلاع","Back up and change boot priority"):app.t("حفظ النسخة وإصلاح GRUB","Back up and repair GRUB");danger:true;enabled:!backend.busy&&confirmation.text===backend.result.confirmation;onClicked:backend.executeBootRepair(confirmation.text)}
 }
}
