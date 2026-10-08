# Verification — 0.4.0

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
- NTFS compressed/encrypted/named streams and ATTRIBUTE_LIST extensions are skipped with reasons. A fragmented `$MFT` works when its full runs fit in its base DATA attribute; external MFT extension records are not followed.
- Deleted FAT32 subdirectory chains are skipped. Surviving live folders and long-name evidence are used; deleted LFN association is tentative. Retained FAT chains may be stale, so FAT32 outputs are candidates.
- GPT/MBR partition addressing assumes 512-byte image sectors; 4Kn partition-table images need conversion or future support. Raw rescue can use 4096-byte sectors.
- Structural scan budgets: 2 million records by default, at most 10 million through the C++ API; bounded GPT tables/EBR chains/directories. Limits generate partial results rather than complete success.
- No overwrite/TRIM reversal, firmware flashing or physical disk repair.
- Source-image size/mtime and raw source serial/size checks do not turn a changing live filesystem into a snapshot.

Hardware-only procedure: [HARDWARE-TESTS-AR.md](HARDWARE-TESTS-AR.md).
