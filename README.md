# NEXVARY Disk Care 0.3.0

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

## Supported limits
Internal disks cannot be formatted by this release. GPT/MBR rebuild erases all target data. Linux provides diagnostics/image operations and read-only surface scanning; physical rescue destination mapping and storage writes currently require Windows. Surface/rescue unreadable-range granularity is 1 MiB and auto-resume is not implemented. Copy readback can be affected by caching; it is not a physical-media certification. Exact NAND capacity, NTFS/exFAT undelete, fragmented-file recovery, physical surface regeneration and vendor firmware flashing are not shipped. The supported recovery and BIOS/hybrid Linux media paths are described below.

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

## Recovery and boot media in 0.3.0

Data rescue now includes PNG/JPEG signature carving from a regular disk-image file. Choose the source image in Data rescue and a destination folder on a recovery disk. Results are placed in a new private folder with a JSON manifest, source offsets, SHA-256 hashes and verified file copies. Cancellation retains completed results. PNG chunks are CRC checked; JPEG end-marker candidates require visual inspection. The 64 MiB per-file and 10,000-file limits bound memory and output. This is not filesystem undelete: original names/folders, fragmented files, overwritten or TRIM-discarded bytes are not restored; allocated photos may also be found.

On Windows, OS installation media offers Windows x64 UEFI/GPT, Windows x64 BIOS + UEFI/MBR, and raw hybrid Linux ISO writing. BIOS mode requires bootmgr and boot/bootsect.exe in a trusted Windows ISO; FAT32 WIM splitting remains available. Linux mode checks ISO9660 and hybrid MBR markers, locks and dismounts target filesystem volumes, writes aligned sectors and verifies image bytes by SHA-256 readback. The selected image determines its actual BIOS/UEFI/Secure Boot compatibility. A marker check cannot prove firmware bootability.

All media writes require administrator access, enumerated external USB/SD/MMC identity and the existing explicit device confirmation. Internal/system/offline/read-only disks remain protected. Files open on the target cause volume-lock failure before raw writing. A write failure after starting can leave incomplete boot media. Hardware boot and recovery on real failing drives remain unverified; use copies and test media first.

A second recovery mode reads deleted FAT32 directory entries from volume images or primary FAT32 MBR partitions. It extracts files up to 64 MiB only when the assumed contiguous clusters remain free in the active FAT. The first short-name character is replaced with an underscore; long names and folder hierarchy are not reconstructed. Fragmentation, overwritten content, GPT, extended partitions, NTFS and exFAT are unsupported. A free allocation entry cannot prove the original content survived; review each recovered file.
