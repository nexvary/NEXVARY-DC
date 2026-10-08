# NEXVARY Disk Care 0.7.0

Adds a separate Linux expert direct-AHCI rescue adapter, backed by a restricted GPL OpenSuperClone 2.5.0 process. Enumeration is limited before MMIO to the chosen controller/port; OS-visible disks/controllers and changed ATA identities are refused. Uses only generated IDENTIFY and READ DMA EXT commands, sector fallback, bounded retries, streaming SHA-256 checkpoints and validated resume.

Optional source-only DCTTech USBRelay2/4/8 power cycling checks unique relay identity/state, two separate +5V/+12V channels and a global cycle budget. Wiring and electrical power restoration still require physical testing. Windows packages include Linux helper sources and instructions, not a native Windows direct-AHCI engine. The separate Linux archive includes the qualified engine, upstream corresponding source, GPL license and patch.

Existing recovery, selective retries, Arabic/English UI, SMART, protected disk operations and boot preparation/repair are preserved and retested. CI gates the release on Linux/Windows verification and real QEMU AHCI reads plus GRUB BIOS/UEFI boot tests.

No HDD family is certified for Firmware/Service Area/Translator repair. Exact models/revisions and validated vendor procedures were not supplied; generic repair scripts are not enabled. The release additionally requires installed Windows 11 LTSC evaluation VM boot in BIOS, UEFI and UEFI Secure Boot, with guest first-logon and Secure Boot state proof. Offline image deployment is used for this fixture; the application's Windows USB preparation has not been tested end to end in these VMs. This fixture does not establish generic Secure Boot compatibility.

See DIRECT-AHCI-AR.md, VERIFICATION.md and HARDWARE-TESTS-AR.md. Manifest pins the delivered commit, Actions run, sizes and SHA-256. Downloading old 0.6.0 assets does not deliver this feature.
