# NEXVARY Disk Care 0.4.0

Adds actual NTFS/exFAT metadata recovery, expanded FAT32/GPT/extended-MBR recovery, streaming extraction and resumable rescue with identity/hash verification and sector retries. Updates Arabic/English controls and installer features. Windows large-WIM staging happens before target erasure and copied parts are SHA-256 verified.

Download Setup.exe or Portable.zip. `release-manifest.json` supplies exact byte sizes, SHA-256, source commit and CI run. Publication waits for Linux/Windows verification, portable startup, installation and removal on the same commit.

Read VERIFICATION.md for tested cases and explicit limits. NTFS compression/encryption/ATTRIBUTE_LIST, deleted FAT32 directory chains and 4Kn GPT image partition tables are unsupported. Complete means byte coverage, not proof against overwrite. No overwritten/TRIM recovery or firmware/physical repair is claimed. Boot helper/readback tests are not full OS boot or Secure Boot verification; those remain unverified.

تجارب الأجهزة الفعلية المطلوبة موضحة في docs/HARDWARE-TESTS-AR.md. النسخة تجريبية للاختبار على صور ووسائط اختبار أولًا.
