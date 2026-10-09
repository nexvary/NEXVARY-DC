import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Panel {
 required property var app
 id:workspace
 property var summary:(app.healthProof || backend.health).summary || ({})
 ColumnLayout {anchors.fill:parent;spacing:13
  RowLayout {Layout.fillWidth:true;AppIcon {name:"boot";Layout.preferredWidth:38;Layout.preferredHeight:38}Label {text:app.t("\u2066Windows\u2069 لا يبدأ — \u20660xc0000001\u2069","Windows will not start — 0xc0000001");font.pixelSize:20;font.bold:true;color:"#edf5fc";Layout.fillWidth:true;wrapMode:Text.WordWrap}}
  Label {text:app.t("مسار تشخيص وإرشاد مع الوصول إلى إنقاذ الصورة وتجهيز وسيط الاسترداد. لا ينفذ هذه الصفحة إصلاح BCD أو SFC تلقائيًا، ولا يثبت رمز الخطأ وحده أن BCD هو السبب.","Diagnostic guidance linked to image rescue and recovery-media preparation. This page does not automatically rebuild BCD or run SFC; the error code alone does not identify BCD as the cause.");color:"#b6ccdc";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Rectangle {visible:summary.rescueFirst===true;Layout.fillWidth:true;implicitHeight:risk.implicitHeight+24;color:"#3a2c22";radius:8
   Label {id:risk;anchors.fill:parent;anchors.margins:12;text:app.t("أولوية الإنقاذ: تظهر مؤشرات تعثر قراءة. في الصورة المرسلة، C5=0x10 يعني 16 قطاعًا معلقًا. تغيير حد التحذير لا يعالجها؛ لا تبدأ إصلاحًا يكتب على الأصل قبل حفظ البيانات.","Rescue priority: read-trouble indicators were reported. In the supplied screenshot C5=0x10 means 16 pending sectors. Changing the warning threshold does not fix them; preserve data before any repair writes to the original.");color:"#ffcd90";wrapMode:Text.WordWrap}
  }
  Repeater {model:[
   {title:app.t("1. احفظ البيانات","1. Preserve data"),body:app.t("من حاسوب آخر أو بيئة حيّة، احفظ صورة القرص على قرص سليم مختلف. احتفظ بخريطة القراءة وتقرير SHA-256. المناطق غير المقروءة تبقى غير مكتملة؛ نفّذ محاولات إضافية مضبوطة فقط عند الحاجة. لا تختبر الكتابة أو الفورمات على النسخة الأصلية.","From another computer or a live environment, save a disk image to a different healthy disk. Retain the read map and SHA-256 report. Unreadable regions remain incomplete; use bounded additional attempts only when needed. Avoid write tests or formatting on the original.")},
   {title:app.t("2. استخدم أدوات الاسترداد","2. Enter recovery tools"),body:app.t("اضغط F1 إذا دخلت WinRE. إن تعذر ذلك، أقلع من وسيط Windows موثوق واختر إصلاح الكمبيوتر بدل تثبيت الآن. اختَر وضع UEFI أو BIOS المطابق للتثبيت الأصلي. احتفظ بمفتاح استرداد BitLocker عند استخدامه؛ لا تحذف القسم لتجاوز القفل.","Use F1 if it enters WinRE. Otherwise boot trusted Windows media and choose Repair your computer. Use the original installation's UEFI/BIOS mode. Have the BitLocker recovery key if needed; do not delete a partition to bypass encryption.")},
   {title:app.t("3. أصلح نسخة سليمة بعد الحفظ","3. Repair a healthy copy after backup"),body:app.t("بعد حفظ البيانات، استخدم استكشاف الأخطاء > خيارات متقدمة > إصلاح بدء التشغيل، ويفضل على نسخة إلى قرص سليم. إذا بدأ الخطأ بعد تحديث أو تعريف، راجع إزالة التحديثات أو استعادة النظام. الحفظ وحده لا يجعل القرص المتعطل سليمًا.","After preserving data, use Troubleshoot > Advanced options > Startup Repair, preferably on a clone on healthy storage. If the failure followed an update or driver change, consider uninstalling updates or System Restore. Backup does not make a failing drive healthy.")},
   {title:app.t("4. إذا استمر الخطأ","4. If the error remains"),body:app.t("راجع نتائج Startup Repair وملف SrtTrail.txt على قسم Windows الفعلي. أحرف الأقسام قد تتغير في WinRE. إصلاح BCD أو فحص ملفات النظام يحتاج تحديد تثبيت Windows وقسم الإقلاع الصحيحين ونسخ الملفات الموجودة أولًا؛ لا تستخدم أوامر فورمات أو bootrec عامة من دون تشخيص. بعد تأمين البيانات، افحص الذاكرة والاتصالات أيضًا.","Review Startup Repair results and SrtTrail.txt on the actual Windows partition. Drive letters can change in WinRE. BCD or system-file repair requires identifying the correct Windows installation and boot partition and preserving existing files first. Avoid generic formatting/bootrec recipes without diagnosis. After data is safe, check memory and connections too.")}
  ];delegate:ColumnLayout {required property var modelData;Layout.fillWidth:true;spacing:5;Label {text:modelData.title;color:"#e4f1fc";font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap}Label {text:modelData.body;color:"#adc6d8";Layout.fillWidth:true;wrapMode:Text.WordWrap}}}
  Flow {Layout.fillWidth:true;spacing:10
   ActionButton {text:app.t("إنقاذ صورة أولًا","Rescue an image first");enabled:!backend.busy&&!app.healthProof;primary:true;onClicked:{app.page=2;app.diskRescueWorkspace=true}}
   ActionButton {text:app.t("تجهيز وسيط Windows","Prepare Windows media");enabled:!backend.busy;onClicked:{app.windowsRecoveryMode=false;app.bootRepairMode=false}}
   ActionButton {text:app.t("قراءة SMART","Read SMART");enabled:!backend.busy;onClicked:app.page=1}
   ActionButton {text:app.t("إرشادات Microsoft","Microsoft guidance");onClicked:Qt.openUrlExternally("https://support.microsoft.com/en-us/windows/experience/startup-boot/startup-repair")}
  }
  Label {text:app.t("وجود C5/C6 لا يثبت وحده سبب 0xc0000001. التجارب على جهازك وقرصه الفعلي تبقى مطلوبة بعد تأهيل البرمجيات.","C5/C6 alone do not prove the cause of 0xc0000001. Your actual machine and disk still require physical verification after software qualification.");color:"#88aac2";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:11}
 }
}
