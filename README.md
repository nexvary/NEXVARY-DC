# NEXVARY Disk Care 0.5.0

C++20 / Qt 6 desktop storage toolkit with Arabic RTL and English. Original colored icon set, developer page and bilingual installer. This remains an experimental build; physical hardware compatibility is not established by CI.

## Implemented
- Disk/partition inventory and selected-device context.
- Bundled Windows smartmontools 7.5; structured SMART temperature, hours, firmware and health/attribute reports when supported.
- Read-only surface scanning; best-effort Windows physical imaging to a verified different physical disk, zero-filled unreadable chunks, read map and destination SHA-256 readback.
- Regular image inspection and exclusive-create verified copying.
- Directory write/read capacity tests with positional patterns; no existing files intentionally overwritten.
- Windows external USB/SD/MMC management: quick partition format, create/delete partitions, destructive GPT/MBR rebuilding, filesystem scan/repair.
- Windows x64 UEFI ISO preparation: validate ISO before erase, GPT/FAT32 up to 31 GiB, SHA-256 copied-file checks, DISM large-WIM splitting.
- Target-specific single-use 180-second confirmation, backup acknowledgement, admin relaunch, disk/partition identity revalidation, internal/system/pagefile protection.
- Async single active job, progress, SQLite history and report export. JSON is hidden behind technical details.

## Recovery and rescue
- NTFS: deleted resident data and full nonresident/sparse runlists; USA fixups, allocation bitmap checks and parent sequence validation. Names and paths are reconstructed when metadata survives.
- exFAT: validated boot and entry-set checksums, allocation bitmap, contiguous extents or retained fragmented FAT chains.
- FAT32: live folder traversal, surviving LFN entries, retained unreferenced FAT chains or contiguous free-cluster candidates. FAT32 evidence remains tentative.
- GPT header/table CRC (backup fallback), MBR primary and logical/extended partitions. Partition tables assume 512-byte sectors.
- All extractors stream contents, removing the old 64 MiB cap. Metadata file limit defaults to 100,000 and is adjustable to 1,000,000; carving stops at 100,000. Structural traversal limits are reported explicitly.
- `complete` means all described bytes extracted; `partial` means missing bytes; `candidate` means heuristic reconstruction. None proves that an original deleted file was not overwritten. SHA-256 verifies copying, not original authenticity.
- Rescue: choose 512/4096-byte logical sector and 0–5 extra attempts. Failed 1 MiB reads split to sectors. Keep `.readmap.jsonl` beside the image; select **Resume existing image** explicitly. The engine validates source identity, destination file identity, contiguous checkpoints and every committed block hash before extending the image. Interrupted, uncommitted tails are safely discarded after validation.

## Supported limits
Internal/system disks cannot be formatted. GPT/MBR rebuilding erases the target. Physical rescue destination mapping and external-media partition mutations require Windows; offline GRUB repair requires live Linux. Raw source serial and actual size must be available; unidentified devices are refused. Surface scan still reports 1 MiB blocks. NTFS compression, encryption, named alternate streams and external MFT ATTRIBUTE_LIST extensions are not reconstructed. Deleted FAT32 directory chains are not guessed. Corrupt/unavailable allocation metadata is reported. Use an immutable source image: size/mtime checks cannot establish a snapshot against all concurrent modifications. No overwritten/TRIM recovery, physical regeneration or generic firmware flashing is claimed.

## Build
Qt **6.8.x** development package (Core, Concurrent, Gui, Qml, Quick, QuickControls2, Sql, Test), CMake >=3.24 and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/nexvary_dc
```

Windows, in an MSVC developer terminal:
```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\nexvary_dc.exe
```

Run `./packaging/build-windows.ps1` from an MSVC developer terminal to deploy Qt, app-local Microsoft runtime and a checksum-pinned smartmontools 7.5 engine with license/source. Compile `packaging/installer.iss` with Inno Setup 6. Windows CI tests portable startup, silent installation, installed startup and uninstallation.

## Data handling
Back up outside the target before writes. Counterfeit storage may corrupt existing files even during free-space tests. Read-only scans can stress failing drives. Cancelled rescue images and read maps are retained; incomplete images are explicitly marked. Storage mutations cannot be cancelled halfway through; do not disconnect the device or close the application.

See [scope and nine observations](docs/OBSERVATIONS.md), [research](docs/RESEARCH.md), [architecture](docs/ARCHITECTURE.md) and [verification](docs/VERIFICATION.md).

## Boot preparation
Windows x64 UEFI/GPT and BIOS+UEFI/MBR paths retain target protection. Large install.wim files are split with DISM into a temporary folder **before erasure**, then each SWM part is copied and SHA-256 compared. This needs temporary free space. Hybrid Linux writing locks target volumes and verifies output by SHA-256. ISO markers and copied files do not establish bootability or Secure Boot support. Boot-mode compatibility depends on the ISO and firmware.

See [0.5.0 verification and limits](docs/VERIFICATION.md), [release notes](docs/RELEASE-0.5.0.md) and [hardware-only checklist in Arabic](docs/HARDWARE-TESTS-AR.md). Release assets include a manifest with the exact tested commit, CI run, byte sizes and SHA-256. The release job only runs after both OS verification jobs succeed.

## Boot repair (0.5.0)
Offline x64 live-Linux GRUB repair is separate from creating installation media. Supports mounted plain ext4 root, BIOS/MBR and UEFI without Secure Boot, verified backups, identity revalidation and a Windows UEFI chainloader entry when its loader survives. [Arabic procedure and limits](docs/BOOT-REPAIR-AR.md). Windows packages include the helper for use from live Linux.

Recovery additionally supports 4Kn GPT images, retained deleted FAT32 folder chains and bounded NTFS file ATTRIBUTE_LIST extensions (external MFT extensions remain unsupported).
