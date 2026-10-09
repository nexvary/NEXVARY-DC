# Verification — 0.7.0

Historical 0.6.0 observations below are retained; the 0.7.0 qualification section supersedes their Windows boot limitation.

Release publication is gated on Windows 2022 and Ubuntu 24.04 builds/tests for the **same commit**. `release-manifest.json` records its SHA and Actions run. A local source tree or a successful compile alone is not release evidence.

## Automated checks in the gate
- NTFS resident and fragmented deleted contents, Unicode parent hierarchy, SHA-256 comparison, USA corruption, allocation collision and invalid geometry.
- A 65 MiB NTFS extraction (above the former 64 MiB cap).
- exFAT contiguous/fragmented deleted contents; entry checksum and bitmap collision rejection.
- GPT CRC-validated exFAT partition extraction and bad-table rejection; bounded EBR traversal.
- FAT32 deleted entries and retained-chain candidates; source preservation.
- PNG CRC, JPEG signature boundaries, malformed/truncated images and cancellation.
- Rescue cancellation/resume, uncommitted image/map tails, changed source identity, corrupted destination, sector fallback/retry count, insufficient destination space and invalid sector settings.
- System/internal/boot/read-only disk write protection and stale identity rejection.
- Arabic/English compact QML startup, confirmation, storage, rescue, boot and developer pages; screenshot artifacts.
- Windows hybrid-writer C# compilation, regular-file fixture write/readback, padding, changed-source rejection and invalid ISO rejection.
- QEMU reference Linux hybrid disk boot to Alpine login in BIOS and UEFI; screenshots and ISO SHA-256 retained.
- Windows portable startup with Qt paths removed, bundled SMART executable, silent installer, installed startup and uninstall.

These deterministic fixtures exercise image data and injected read failures. They do not emulate every filesystem implementation, controller, filesystem race or physical failure. Parser fuzzing, exhaustive filesystem coverage and hardware certification are not claimed.

## Boot verification boundary
Windows BIOS/UEFI preparation is implemented. WIM parts are staged before erasure and hash-verified after copying. Hybrid Linux writes have byte-for-byte readback. No bootable Windows ISO is supplied to this session; no full Windows installer boot in a VM is attested. GitHub Actions additionally boots the official Alpine virt 3.22.1 hybrid ISO as a virtual hard disk under QEMU TCG with SeaBIOS and OVMF UEFI, requiring an identifiable Alpine login screen (OCR plus retained screenshots). Both modes reached login in run 37783893889. This validates that reference image in those virtual firmware modes; it is not an end-to-end Windows USB creation or Secure Boot test. Secure Boot is unverified for all modes. This is a software-validation limitation, not a hardware test silently passed to the user.

## Remaining implementation limits
- NTFS compressed/encrypted/named streams and external MFT ATTRIBUTE_LIST extensions are skipped with reasons. A fragmented `$MFT` works when its full runs fit in its base DATA attribute; external MFT extension records are not followed.
- Deleted FAT32 subdirectory chains are traversed only with retained FAT allocation and valid dot entries; cleared chains are skipped. Surviving live folders and long-name evidence are used; deleted LFN association is tentative. Retained FAT chains may be stale, so FAT32 outputs are candidates.
- GPT image partitions support CRC-validated 512-byte and 4096-byte sectors with backup-header fallback. MBR addressing assumes 512-byte image sectors. Raw rescue can use 4096-byte sectors.
- Structural scan budgets: 2 million records by default, at most 10 million through the C++ API; bounded GPT tables/EBR chains/directories. Limits generate partial results rather than complete success.
- No overwrite/TRIM reversal, firmware flashing or physical disk repair.
- Source-image size/mtime and raw source serial/size checks do not turn a changing live filesystem into a snapshot.

Hardware-only procedure: [HARDWARE-TESTS-AR.md](HARDWARE-TESTS-AR.md).

## 0.5.0 additional gates
- NTFS file ATTRIBUTE_LIST fragmentation with sequence and extension base-reference rejection.
- 4Kn GPT primary/backup recovery and corrupt-both rejection with original content/SHA-256 comparisons.
- Retained deleted FAT32 directory traversal and cleared-chain refusal.
- Live-Linux GRUB helper safety checks: stale/expired plans, exact confirmation, backup-space guard and unsafe symlinks.
- QEMU BIOS/MBR and OVMF UEFI repaired-disk tests: initial disk has no GRUB loader, helper repairs from a RAM-based live guest, then firmware boots the disk and GRUB hands off to a Linux kernel. Existing Windows firmware entry and loader placeholder are preserved; this does not boot Windows. Secure Boot is disabled.

GRUB repair supports plain ext4 root with /boot inside it and an intact grub.cfg. It does not support separate /boot, encrypted/LVM/RAID/Btrfs roots, BIOS/GPT, missing/corrupt menu configuration or Windows BCD rebuilding. No automatic raw-sector rollback is claimed; verified backups and failure reports are retained.

Windows firmware-priority repair is limited to an existing GRUB/shim entry in the current firmware order. It preserves other entries and makes no partition writes. BCDEdit displayorder parsing must be recognized; otherwise it refuses safely. Parser/localization fixtures do not certify real device firmware writes.

Windows visual proof uses the native Windows platform, not the font-less offscreen test platform. The screenshot command checks required Arabic/Latin glyph coverage before capturing. Offscreen startup tests are still lifecycle checks, not font-rendering evidence.

## 0.6.0 gates
- Selective failed-sector retry: original image/map preservation, healthy physical
  sectors never reread, newly recovered content and SHA-256, cancellation/resume,
  remaining failures and refusal of changed identity/corrupted evidence before disk reads.
- SMART-derived capability fixtures: unknown data remains unknown; a WD model or
  SAT bridge never enables firmware repair. No certified device firmware profiles.
- Separately builds pinned OpenSuperClone v2.5.0 (87a25d257e44337ae15f863e59b0062311a5c329),
  verifies help/version, retains license and exact upstream source archive. Does not
  install/load its driver, access a disk, execute its scripts or certify direct AHCI/relay.

## 0.7.0 direct AHCI qualification

Nine additional deterministic Python tests cover streaming SHA-256, cancellation/resume, crash tails, bad-sector fallback/retry budget, corruption, changed identities, image bounds, insufficient space, OS-visible controller refusal and USBRelay transfer errors/restoration. A separate QEMU AHCI test performs actual MMIO/DMA reads through the restricted engine and checks SHA-256 and resumed output. Source is attached read-only on the host. Physical SATA timing, DMA compatibility and electrical relay operation remain hardware tests. No firmware repair family is certified; release qualification requires installed Windows evaluation boot in BIOS, UEFI and UEFI Secure Boot with state confirmation inside the guest. The application's Windows USB creation remains without an end-to-end VM boot test.

## 0.7.0 Windows production USB qualification

Release publication additionally requires BIOS, UEFI and UEFI Secure Boot guests using one pinned official Windows 11 LTSC evaluation ISO. Each reaches installed Windows first logon, runs the production storage backend on an isolated emulated USB, rejects the system disk, stale serial and a 1 GiB target before mutation, verifies copied bytes, then boots the prepared USB to Windows PE. A test-only batch-shell hook in boot.wim reports kernel/firmware state on a separate disposable FAT USB; signed EFI executables remain unchanged. All mode results must report success in the release manifest. Fixture presence alone is not proof of a successful run. This does not certify all Windows ISOs, physical firmware or Secure Boot key/revocation databases.

Windows qualification runs under KVM on an ephemeral Linux CI host. A test-only batch hook in the evaluation ISO boot.wim deploys the guest using Windows PE DISM/BCDBoot. The original pinned ISO is copied unchanged into the installed guest for the production USB tests. No Microsoft ISO, WIM or installed disk is published; only serial/guest text logs, screenshots and result JSON are retained. The former Windows-host TCG fixture is retained as a diagnostic alternative; its timeout is not successful qualification.
