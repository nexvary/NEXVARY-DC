# NEXVARY Disk Care 0.2.0

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
Internal disks cannot be formatted by this release. GPT/MBR rebuild erases all target data. Linux provides diagnostics/image operations and read-only surface scanning; physical rescue destination mapping and storage writes currently require Windows. Surface/rescue unreadable-range granularity is 1 MiB and auto-resume is not implemented. Copy readback can be affected by caching; it is not a physical-media certification. Exact NAND capacity, deleted-file recovery, physical surface regeneration, vendor firmware flashing, BIOS boot and Linux ISO writing are not shipped.

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
