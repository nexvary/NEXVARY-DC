# إصلاح إقلاع Linux بعد تثبيت Windows — 0.5.0

هذا مسار مستقل عن الفورمات وتجهيز فلاشة التثبيت. يستخدم `grub-install` من
بيئة Linux حية لإعادة تثبيت GRUB، ويحافظ على قائمة Linux الموجودة. في UEFI
يضيف قائمة Windows إذا وجد ملفه في EFI، وينشئ إدخال NEXVARY Linux في الفيرموير
مع الاحتفاظ بإدخالات Windows وترتيبها النسبي. لا يهيئ الأقسام ولا يحذفها.

الدعم: x86_64، قسم جذر ext4 عادي يحتوي `/boot` وملف `grub.cfg` سليم؛
BIOS على MBR مع مساحة تضمين 1 MiB، أو UEFI مع قسم EFI على القرص نفسه وSecure
Boot معطل. لا يدعم جذر LUKS/LVM/RAID أو Btrfs، ولا `/boot` المنفصل، ولا إصلاح
تلف ملف القائمة أو إعادة إنشاء ملفات Windows BCD المفقودة. لا تستخدم هذا المسار
إذا حُذف قسم Linux أو كُتب فوقه أثناء تثبيت Windows.

## الاستخدام من Ubuntu حي

1. أقلع من وسيط Ubuntu واختر تجربة النظام. في UEFI أقلع الوسيط في نمط UEFI.
2. ثبّت الأدوات إذا لم تتوفر:

```bash
sudo apt update
sudo apt install python3 grub2-common grub-pc-bin grub-efi-amd64-bin efibootmgr
lsblk -o NAME,SIZE,FSTYPE,UUID,PARTUUID,MOUNTPOINTS
```

3. حدد قسم Linux وEFI والقرص الكامل بنفسك. المثال التالي **قيم توضيحية**؛
   استبدلها بالقيم المطابقة لجهازك. تركيب ext4 للكتابة قد يعيد تشغيل journal؛
   افحص القرص أو أنشئ صورة إنقاذ أولًا إذا كان مصدرًا لفحص جنائي أو متعطلًا.

```bash
sudo mkdir -p /mnt/linux /mnt/esp
sudo mount /dev/nvme0n1p3 /mnt/linux
sudo mount /dev/nvme0n1p1 /mnt/esp
```

4. شغّل نسخة Linux من البرنامج بصلاحية root من البيئة الحية، واذهب إلى
   الإقلاع والتثبيت ثم إصلاح GRUB. أدخل مساري التركيب والقرص الكامل ومجلدًا
   موجودًا للنسخة الاحتياطية على قرص آخر. يمكن استخدام الأداة المرفقة من
   الطرفية بدل الواجهة:

```json
{"action":"plan","root":"/mnt/linux","esp":"/mnt/esp","disk":"/dev/nvme0n1","mode":"uefi","backup":"/media/ubuntu/BACKUP"}
```

```bash
sudo python3 boot_repair.py --request request.json > plan-result.json
```

   راجع `plan-result.json`. لتنفيذ CLI، ضع كائن `plan` الناتج نفسه في طلب جديد:

```json
{"action":"apply","plan":{},"confirmation":"REPAIR /dev/nvme0n1"}
```

   استبدل `{}` بخطة الفحص كاملة. تنتهي صلاحيتها بعد 180 ثانية. تحتاج خطة جديدة
   إذا تغيّرت هوية القرص أو الأقسام أو القائمة أو حالة الفيرموير.

5. التنفيذ يحفظ نسخ ملفات GRUB وEFI مع SHA-256، ومعلومات الفيرموير. في BIOS
   يحفظ أول 1 MiB أيضًا. احتفظ بمجلد النسخة الذي يعرضه التقرير على وسيط خارجي.
   لا يَعِد البرنامج بتراجع تلقائي كامل عند فشل أداة خارجية؛ تُحفظ النسخة
   والتقرير للمراجعة، ولا يعيد كتابة قطاعات قد تغيّرت عشوائيًا.
6. افصل الوسيط الحي وأعد التشغيل. جرّب Linux وWindows كلًا على حدة.
   نجاح تثبيت GRUB وفحص القائمة لا يثبت نجاح إقلاع Windows أو Secure Boot.

لـBIOS اختر `mode: "bios"` واترك `esp` فارغًا؛ يحتفظ المسار بإدخالات القائمة
الموجودة، ولا يخمّن قسم إقلاع Windows أو يعيد كتابة محمّله.

## أساس التنفيذ

- [GNU GRUB: Installing GRUB](https://www.gnu.org/software/grub/manual/grub/html_node/Installing-GRUB-using-grub_002dinstall.html)
- [Ubuntu: Recovering Ubuntu after installing Windows](https://help.ubuntu.com/community/RecoveringUbuntuAfterInstallingWindows)

لا يُضمّن هذا المشروع GRUB داخل البرنامج؛ الأدوات يوفّرها النظام الحي وتخضع
لتراخيص توزيعتها. الأداة الأصلية في المشروع تستخدم مكتبة Python القياسية.

## تغيير أولوية Linux من Windows

إذا ظل إدخال GRUB/shim موجودًا في UEFI، شغّل البرنامج كمسؤول وافحص إدخالات Linux ثم راجع خطة تقديم الإدخال المحدد. تُحفظ نسخة من BCD وترتيب الفيرموير، ولا تُكتب ملفات الأقسام. هذا المسار لا يعيد ملفات EFI المفقودة ولا يصلح GRUB نفسه. يرفض ترتيبًا لا يستطيع تفسيره بأمان، بما فيه لغات مخرجات BCDEdit التي لا تتوفر لها قراءة حقل displayorder. أدخل تأكيد BOOT ومعرّف الإدخال كما يظهر في الخطة. ثم أعد التشغيل للتحقق.
