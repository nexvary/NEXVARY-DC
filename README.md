# NEXVARY Disk Care

Independent C++20 / Qt 6 desktop storage toolkit. **0.1.0 is a foundation preview, not a finished disk-repair product.** Arabic RTL and English UI.

## Implemented
- Mounted-volume enumeration, sizes/free space; physical-disk enumeration through read-only OS tools.
- Optional external `smartctl --json --all` health report for an enumerated disk. No bundled smartctl; unavailable/permission-denied is reported.
- Read-only regular-image scan with SHA-256.
- Exclusive-create regular-image copy, source hash, independent destination readback/hash, cancellation and incomplete-copy cleanup.
- Acknowledged directory write/read test with unique position patterns, flush/sync, and test-file cleanup; detects corruption and wrapped-address behavior in tests.
- Asynchronous progress, one active job, SQLite history, JSON export.

## Not yet implemented
Raw disk rescue, partition/file recovery, raw surface tests, sector treatment, erase/sanitize, firmware updates and exact raw capacity probing. These workspaces explain their status. Passing a bounded directory test is **not proof of full device capacity**, and does not mean damaged media is new.

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

Use `packaging/build-windows.ps1` after building to produce a portable folder. CI uploads that folder and attempts an Inno Setup installer. Local Linux tests do not establish a Windows build result.

Optional SMART engine: install smartmontools from its official project/distribution package and add smartctl to PATH. Administrative rights may be necessary. No engine is silently downloaded.

## Data handling
Back up before write tests. Counterfeit storage can corrupt existing data even when tests write only in free space. A directory test checks only allocated test bytes. Only newly created test files are removed by the app. Disk source images should be immutable during use. The preview cannot repair physical damage.

See [product scope](docs/PRODUCT.md), [research](docs/RESEARCH.md), [architecture](docs/ARCHITECTURE.md) and [verification](docs/VERIFICATION.md).
