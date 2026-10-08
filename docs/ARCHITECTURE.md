# Architecture and failure planning

## Boundaries
- `dc_core`: UI-independent file operations and test algorithm, cancellation and progress callbacks.
- `Controller`: asynchronous job orchestration, discovery adapters, bundled fixed-command SMART read, SQLite history and JSON exports.
- `qml`: navigation and AR/EN presentation; no disk commands inside QML.
- Windows storage operations run a trusted, resource-embedded PowerShell helper with a base64 JSON request. No user command is evaluated. Whole-application UAC relaunch is currently required; a dedicated privilege broker remains future work.

## Current safety invariants
- Regular-image operations require regular files and reject raw paths/symlinks. Physical-media operations accept only an enumerated path and use a separate read-only backend. Linux validates the opened descriptor as a regular file.
- Image destinations created exclusively; existing files never truncated. Copy computes SHA-256 and independently reads the destination to compare. Incomplete/cancelled copies are removed.
- Directory tests use a new private temporary directory and exclusive per-block file creation. Only that test directory is cleaned up.
- All patterns are position-specific and per-job nonce-specific; all writes precede verification to catch wrapping fake media.
- File data is flushed and synchronized before verification. OS/controller caching and firmware dishonesty remain limitations. No claim of durable, power-cycle-verified physical capacity.
- A 64 MiB free-space reserve is enforced. Concurrent outside writes can still exhaust space; write failure yields an error, not a capacity verdict.
- One job at a time. Worker-thread operations report via queued signals. Closing during a read job requests cancellation and keeps the UI alive. Storage mutations disable cancellation and close until the Windows service completes.
- SMART permits only enumerated paths and fixed read-only arguments, without a shell. Read-only SMART reconnect identity validation remains a follow-up; physical source handles are also read-only. Windows mutation helpers revalidate unique ID, serial, disk length and selected partition offsets/sizes.
- Reports retain scope, operation, timestamps, status and version. Database failures do not suppress the current result.

## Anticipated failures
| Risk | Current or planned response |
|---|---|
| USB bridge hides SMART/ATA commands | report unavailable; never infer healthy |
| Disk identity/path changes after reconnect | Windows write helper revalidates disk and partition identity immediately before mutation; physical read handles remain read-only |
| System disk selected for erase | Windows helper rejects boot/system/offline/read-only/internal disks and disks holding pagefiles, source ISO, application or user profile |
| Permission/UAC denied | current read engines report errors; user-invoked UAC relaunch; denied elevation does not start mutation |
| Image changes during reading | detect size changes and short read; same-size external modification is not fully detected, so use an immutable image |
| Power loss during copying | destination may remain partial after process/crash/power loss; only completed report + verified SHA establishes success; rescue uses durable JSONL checkpoints; regular image copying remains restart-only |
| Counterfeit flash wraps existing allocations | explicit backup requirement before any write test; test-file isolation cannot protect data from dishonest firmware |
| Windows raw locks, 4Kn alignment, NVMe namespaces | reads use 1 MiB blocks; physical length must be 512-byte aligned. 4Kn, bridge and physical-device behavior still need hardware tests |
| CI lacks physical hardware | fault-injection tests and image fixtures only; physical success remains unverified |
| UI/build dependency failures | Linux and Windows CI, offscreen UI startup, portable packaging |
| Future Android request | outside current scope; raw disk access is not assumed to work through Android permissions |

## Persistence
SQLite stores the last 50 displayed records (database itself retains all records). Paths, serials and SMART output stay local. Export JSON is user-initiated. No telemetry or cloud service.

## Storage confirmation
Preparation snapshots enumerated metadata and creates a single-use token valid for 180 seconds. Execution consumes the token and requires exact device-path text plus acknowledgement. The trusted helper re-enumerates before commands and compares identifiers, capacity and partition offsets/sizes. Only external USB/SD/MMC media are writable. The source ISO is mounted and inspected before erasure. Mutation completion is not treated as proof of firmware or physical-surface repair.

## Legacy 0.3 rescue imaging (superseded)
Unreadable 1 MiB chunks are zero-filled and listed explicitly. Cancellation retains a partial image and read map. Read maps are not auto-resume maps. A full output reread compares SHA-256 with the written stream; destination extents must exclude the source physical disk. Test fixtures use regular files, not physical drives. No reads are claimed to be snapshots of a live changing filesystem.

## 0.4 recovery architecture
`Partitions` bounds GPT/MBR/EBR extents. `FilesystemRecovery` reads NTFS, exFAT and FAT32 metadata independently of the UI, validates allocation and streams recovered content in buffers of at most 1 MiB. Bounded directory/MFT indexes and runlists consume metadata memory; contents are not held in full. `Recovery` streams signature candidates and PNG CRC checks.

`RescueEngine` accepts a read-only transport. Each committed 1 MiB block is flushed and synchronized before its offset/hash/bad-sector row is flushed and synchronized to JSONL. On resume the header's source and destination identities are compared; all committed output bytes and range coverage are validated before modifying uncommitted tails. A final independent reread compares the whole output hash. A crash may lose the currently uncommitted block; it is reread, never assumed successful. Firmware/controller caching remains a physical limitation.

No new third-party recovery binary/library is bundled. The implementation uses documented filesystem structures; upstream implementation code was not copied. See RESEARCH.md for structure references. Native parsers intentionally reject unsupported NTFS attribute-list extensions, encryption and compression rather than guessing physical runs.
