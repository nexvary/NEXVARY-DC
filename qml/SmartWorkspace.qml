import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Panel {
 id:workspace
 required property var app
 property var snapshot:app.healthProof || backend.health
 property var summary:snapshot.summary || ({})
 property var preferences:backend.smartPreferences
 property bool proof:!!app.healthProof
 function value(v){return v===undefined||v===null?"—":String(v)}
 function healthLabel(){return summary.health==="failed"?app.t("خطر","Bad"):summary.health==="caution"?app.t("تحذير","Caution"):summary.health==="passed"?app.t("اجتاز SMART","SMART passed"):app.t("غير معروفة","Unknown")}
 function healthColor(){return summary.health==="failed"?"#f78276":summary.health==="caution"?"#f4cc6a":summary.health==="passed"?"#73d8b0":"#9ab0c2"}
 function setting(key,v){let values={};values[key]=v;backend.setSmartPreferences(values)}
 ColumnLayout {anchors.fill:parent;spacing:12
  RowLayout {Layout.fillWidth:true
   AppIcon {name:"disk";Layout.preferredWidth:38;Layout.preferredHeight:38}
   Label {text:app.t("صحة القرص والمراقبة","Drive health & monitoring");font.pixelSize:20;font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#edf5fc"}
   ActionButton {text:app.t("قراءة SMART","Read SMART");primary:true;enabled:!backend.busy&&!workspace.proof&&!!app.selectedDisk.device;onClicked:backend.inspectHealth(app.selectedDisk.device)}
  }
  Label {visible:workspace.proof;text:app.t("بيانات اختبار معلّمة — ليست قراءة من جهاز حقيقي","LABELLED TEST DATA — not a physical device reading");color:"#f2cc7d";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  Label {text:summary.model || app.t("اختر القرص واقرأ بياناته أولًا","Select a disk and read its data first");color:"#e6f2ff";font.bold:true;font.pixelSize:19;Layout.fillWidth:true;wrapMode:Text.WordWrap}
  GridLayout {Layout.fillWidth:true;columns:3;columnSpacing:10;rowSpacing:10
   Repeater {model:[{title:app.t("الحالة","Health"),text:workspace.healthLabel(),color:workspace.healthColor()},{title:app.t("الحرارة","Temperature"),text:summary.temperature===undefined||summary.temperature===null?"—":preferences.fahrenheit?(Number(summary.temperature)*9/5+32).toFixed(1)+" °F":summary.temperature+" °C",color:"#7fc6ef"},{title:app.t("القطاعات المعلقة C5","Pending sectors C5"),text:workspace.value(summary.pending),color:Number(summary.pending)>0?"#f4cc6a":"#dce9f4"}]
    delegate:Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:85;radius:9;color:"#0b1a28";border.color:modelData.color
     ColumnLayout {anchors.fill:parent;anchors.margins:10;spacing:6;Label {text:modelData.title;color:"#a8bfd0";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:12}Label {text:modelData.text;color:modelData.color;font.bold:true;font.pixelSize:21;Layout.fillWidth:true;elide:Text.ElideRight}}
    }
   }
  }
  Label {visible:summary.rescueFirst===true;text:app.t("أنقذ البيانات أولًا. C5 وC6 مؤشرات تعثر قراءة؛ رفع حد التحذير لا يصلح القرص. لا تبدأ CHKDSK /r أو فورمات أو اختبار كتابة على الأصل قبل النسخ. ظهور 0xc0000001 قد يصاحب تلف ملفات الإقلاع أو أسباب أخرى.","Rescue data first. C5/C6 indicate read trouble; raising a warning threshold does not repair the drive. Avoid CHKDSK /r, formatting or write tests on the original before backup. Error 0xc0000001 can involve boot-file damage or other causes.");color:"#ffcf93";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  RowLayout {Layout.fillWidth:true
   ActionButton {text:app.t("إنقاذ صورة القرص","Rescue disk image");enabled:!backend.busy&&!workspace.proof;onClicked:{app.page=2;app.diskRescueWorkspace=true}}
   ActionButton {visible:backend.windows;text:app.t("حلول تعطل Windows","Windows recovery solutions");enabled:!backend.busy&&!workspace.proof;onClicked:{app.page=8;app.windowsRecoveryMode=true;app.bootRepairMode=false}}
  }
  GridLayout {columns:2;Layout.fillWidth:true;columnSpacing:18;rowSpacing:8
   Repeater {model:[{name:app.t("Firmware","Firmware"),v:summary.firmware},{name:app.t("الرقم التسلسلي","Serial"),v:preferences.hideSerial&&summary.serial?"••••••••":summary.serial},{name:app.t("ساعات التشغيل","Power-on hours"),v:summary.hours},{name:app.t("مرات التشغيل","Power cycles"),v:summary.powerCycles},{name:app.t("الواجهة","Interface"),v:summary.interface},{name:app.t("سرعة الدوران","Rotation rate"),v:summary.rotationRate},{name:app.t("قطاعات معاد تخصيصها 05","Reallocated 05"),v:summary.reallocated},{name:app.t("غير قابلة للتصحيح C6","Uncorrectable C6"),v:summary.uncorrectable},{name:app.t("آخر قراءة UTC","Last read UTC"),v:snapshot.capturedAt},{name:app.t("المحرك","Engine"),v:snapshot.engine||"smartctl"}]
    delegate:RowLayout {required property var modelData;Layout.fillWidth:true;Label {text:modelData.name;color:"#8eafc6";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:12}Label {text:workspace.value(modelData.v);color:"#dce9f4";Layout.maximumWidth:190;elide:Text.ElideRight;LayoutMirroring.enabled:false}}
   }
  }
  RowLayout {Layout.fillWidth:true
   CheckBox {text:app.t("إخفاء الرقم في العرض","Hide serial in display");checked:preferences.hideSerial===true;enabled:!backend.busy;onToggled:workspace.setting("hideSerial",checked)}
   CheckBox {text:"°F";checked:preferences.fahrenheit===true;enabled:!backend.busy;onToggled:workspace.setting("fahrenheit",checked)}
   CheckBox {text:app.t("قيم سداسية","Hex raw");checked:preferences.hexRaw!==false;enabled:!backend.busy;onToggled:workspace.setting("hexRaw",checked)}
  }
  Label {text:app.t("إخفاء الرقم يخص العرض فقط؛ تقرير JSON يحتفظ بالهوية للتحقق من المصدر.","Hiding the serial affects display only; JSON reports retain identity for source verification.");color:"#8eabc1";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:11}
  GridLayout {Layout.fillWidth:true;columns:4;columnSpacing:8;rowSpacing:8
   Label {text:app.t("تنبيه الحرارة °C","Temperature alert °C");color:"#b8ccda";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   SpinBox {from:20;to:90;value:preferences.temperatureAlert||55;enabled:!backend.busy;onValueModified:workspace.setting("temperatureAlert",value)}
   Label {text:app.t("التحديث بالثواني","Refresh seconds");color:"#b8ccda";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   SpinBox {from:60;to:3600;stepSize:60;value:preferences.refreshSeconds||300;enabled:!backend.busy;onValueModified:workspace.setting("refreshSeconds",value)}
   Repeater {model:[{id:"reallocatedAlert",label:"05"},{id:"pendingAlert",label:"C5"},{id:"uncorrectableAlert",label:"C6"}]
    delegate:RowLayout {required property var modelData;Layout.columnSpan:2;Layout.fillWidth:true;Label {text:app.t("حد التنبيه ","Alert threshold ")+modelData.label;color:"#b8ccda";Layout.fillWidth:true}SpinBox {from:1;to:65535;editable:true;value:preferences[modelData.id]||1;enabled:!backend.busy;onValueModified:workspace.setting(modelData.id,value)}}
   }
   CheckBox {text:app.t("مراقبة القرص أثناء فتح البرنامج","Monitor selected disk while this app is open");checked:backend.monitoring;enabled:!backend.busy&&!workspace.proof;Layout.columnSpan:2;onToggled:backend.setMonitoring(checked)}
  }
  Label {text:app.t("حدود التنبيه لا تغيّر حالة الخطر ولا بيانات SMART. المراقبة تتوقف عند فشل القراءة أو تغيّر الهوية، وتتخطى دورات التحديث أثناء العمليات الأخرى.","Alert limits change notifications, not SMART data or the underlying risk. Monitoring stops on read failure or identity change and skips polls while other operations run.");color:"#afc4d4";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:11}
  RowLayout {Layout.fillWidth:true
   Label {text:app.t("سجل الاتجاهات","Recorded trends");color:"#e0edf7";font.bold:true;Layout.fillWidth:true}
   ComboBox {id:metric;model:[app.t("الحرارة","Temperature"),"C5","C6","05"];property var keys:["temperature","pending","uncorrectable","reallocated"];onCurrentIndexChanged:graph.requestPaint()}
  }
  Canvas {id:graph;Layout.fillWidth:true;Layout.preferredHeight:160;property var samples:backend.healthSamples;onSamplesChanged:requestPaint();onWidthChanged:requestPaint()
   onPaint:{let ctx=getContext("2d");ctx.fillStyle="#091725";ctx.fillRect(0,0,width,height);let pts=samples.filter(s=>s[metric.keys[metric.currentIndex]]!==undefined&&s[metric.keys[metric.currentIndex]]!==null);ctx.fillStyle="#adc4d7";ctx.font="12px sans-serif";if(pts.length<2){ctx.fillText(app.t("يلزم قراءتان مسجلتان لرسم الاتجاه","Two recorded samples are needed"),18,30);return}let vals=pts.map(s=>Number(s[metric.keys[metric.currentIndex]]));let lo=Math.min.apply(null,vals),hi=Math.max.apply(null,vals);if(hi===lo)hi=lo+1;ctx.fillText(String(lo)+" — "+String(hi),16,20);ctx.strokeStyle="#68c2ed";ctx.lineWidth=2;ctx.beginPath();for(let i=0;i<vals.length;i++){let x=18+(width-36)*i/(vals.length-1),y=height-18-(height-50)*(vals[i]-lo)/(hi-lo);if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)}ctx.stroke();}
  }
  Label {text:app.t("جدول SMART الكامل","Full SMART attribute table");color:"#edf4fc";font.bold:true;Layout.fillWidth:true}
  RowLayout {Layout.fillWidth:true;Label {text:"ID";Layout.preferredWidth:35;color:"#88afca"}Label {text:app.t("المؤشر","Attribute");Layout.fillWidth:true;color:"#88afca"}Label {text:app.t("حالي","Current");Layout.preferredWidth:48;color:"#88afca"}Label {text:app.t("أسوأ","Worst");Layout.preferredWidth:48;color:"#88afca"}Label {text:app.t("حد","Thresh");Layout.preferredWidth:48;color:"#88afca"}Label {text:preferences.hexRaw?"RAW HEX":"RAW DEC";Layout.preferredWidth:126;color:"#88afca"}}
  Repeater {model:summary.attributes||[];delegate:Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:Math.max(42,attributeRow.implicitHeight+14);color:modelData.warning?"#3b3023":"#0c1a27";radius:5
   RowLayout {id:attributeRow;anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.margins:7;Label {text:modelData.hexId||workspace.value(modelData.id);Layout.preferredWidth:35;color:"#b7d4e9"}Label {text:modelData.name||"";Layout.fillWidth:true;wrapMode:Text.WordWrap;color:modelData.warning?"#f5ce7e":"#c2d4e1";font.pixelSize:12}Label {text:workspace.value(modelData.value);Layout.preferredWidth:48;color:"#b6ccdd"}Label {text:workspace.value(modelData.worst);Layout.preferredWidth:48;color:"#b6ccdd"}Label {text:workspace.value(modelData.threshold);Layout.preferredWidth:48;color:"#b6ccdd"}Label {text:preferences.hexRaw?modelData.rawHex||workspace.value(modelData.raw):workspace.value(modelData.raw);Layout.preferredWidth:126;color:"#efc979";font.pixelSize:12;elide:Text.ElideRight;LayoutMirroring.enabled:false}}
  }}
  Label {visible:!(summary.attributes&&summary.attributes.length);text:app.t("لا توجد بيانات مؤشرات مؤكدة لهذه القراءة.","No verified attribute data in this reading.");color:"#afc4d4";Layout.fillWidth:true;wrapMode:Text.WordWrap}
  ColumnLayout {visible:backend.windows;Layout.fillWidth:true;spacing:10
   Label {text:app.t("محرك CrystalDiskInfo الكامل","Complete CrystalDiskInfo engine");color:"#eff5fc";font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap}
   Label {text:app.t("قراءة ثانية بمحرك 9.9.2 المضمّن؛ دعم الأجهزة يعتمد على المتحكم والمحول. اللوحة المرخصة تتيح AAM/APM والرسوم والتنبيهات والبريد والإقامة في شريط النظام والإعدادات المتقدمة. قد يفعّل المحرك SMART أثناء الاكتشاف. التحكم في الإعدادات لا يصلح تلفًا ماديًا.","Second reading with the bundled 9.9.2 engine; compatibility depends on controller and bridge. Its licensed panel provides AAM/APM, graphs, alarms, mail, resident tray monitoring and advanced settings. The original engine may enable SMART during detection. Configuration controls do not repair physical damage.");color:"#afc4d4";Layout.fillWidth:true;wrapMode:Text.WordWrap}
   RowLayout {Layout.fillWidth:true;ActionButton {text:app.t("قراءة بالمحرك المضمّن","Read with bundled engine");enabled:!backend.busy&&!workspace.proof&&backend.administrator;onClicked:backend.readCrystalHealth()}ActionButton {text:app.t("اللوحة المتقدمة الكاملة","Full advanced panel");enabled:!backend.busy&&!workspace.proof&&backend.administrator;onClicked:advanced.open()}}
   ComboBox {id:crystalChoice;visible:backend.crystalDisks.length>0;Layout.fillWidth:true;model:backend.crystalDisks.map(d=>(d.summary.model||"—")+" · "+(preferences.hideSerial?"••••••":d.summary.serial||"—"));onActivated:backend.selectCrystalDisk(currentIndex)}
  }
  Dialog {id:advanced;parent:Overlay.overlay;modal:true;anchors.centerIn:parent;width:Math.min(620,app.width-100);title:app.t("اللوحة المتقدمة","Advanced panel")
   contentItem:Label {LayoutMirroring.enabled:app.arabic;LayoutMirroring.childrenInherit:true;horizontalAlignment:app.arabic?Text.AlignRight:Text.AlignLeft;text:app.t("ستفتح لوحة المحرك الأصلي المرخص في نافذة مستقلة. تغيير AAM/APM يؤثر في الطاقة والأداء ويحتاج قرصًا يدعمه؛ لا تستخدمه كعلاج للباد. التطبيق يعطل التطبيق التلقائي لإعدادات AAM/APM عند فتح اللوحة. لن تُرسل رسائل بريد إلا بعد إعداد الخدمة واختيارها داخل اللوحة.","The licensed upstream panel opens in its own window. AAM/APM changes power/performance and requires supported hardware; it is not a bad-sector remedy. Automatic AAM/APM adaptation is disabled when opening the panel. Mail is sent only after you configure and enable it there.");wrapMode:Text.WordWrap;color:"#c4d6e5"}
   footer:RowLayout {LayoutMirroring.enabled:app.arabic;LayoutMirroring.childrenInherit:true;spacing:10;ActionButton {text:app.t("فتح اللوحة","Open panel");primary:true;onClicked:advanced.accept()}ActionButton {text:app.t("إلغاء","Cancel");onClicked:advanced.reject()}}
   onAccepted:backend.openCrystalPanel(app.arabic,true)
  }
 }
}
