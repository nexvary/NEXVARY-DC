# Architecture and failure planning

## Boundaries
- `dc_core`: UI-independent file operations and test algorithm, cancellation and progress callbacks.
- `Controller`: asynchronous job orchestration, discovery adapters, optional fixed-command SMART read, SQLite history and JSON exports.
- `qml`: navigation and AR/EN presentation; no disk commands inside QML.
- Later: dedicated privilege broker and device registry. No raw write engine is shipped now.

## Current safety invariants
- Image sources must be regular files; raw device paths and symlinks rejected. Linux validates the opened descriptor as a regular file.
- Image destinations created exclusively; existing files never truncated. Copy computes SHA-256 and independently reads the destination to compare. Incomplete/cancelled copies are removed.
- Directory tests use a new private temporary directory and exclusive per-block file creation. Only that test directory is cleaned up.
- All patterns are position-specific and per-job nonce-specific; all writes precede verification to catch wrapping fake media.
- File data is flushed and synchronized before verification. OS/controller caching and firmware dishonesty remain limitations. No claim of durable, power-cycle-verified physical capacity.
- A 64 MiB free-space reserve is enforced. Concurrent outside writes can still exhaust space; write failure yields an error, not a capacity verdict.
- One job at a time. Worker-thread operations report via queued signals. Closing during a job requests cancellation and keeps the UI alive.
- SMART permits only enumerated paths and fixed read-only arguments, without a shell. Read-only SMART reconnect identity validation remains a follow-up; no write operation uses these handles.
- Reports retain scope, operation, timestamps, status and version. Database failures do not suppress the current result.

## Anticipated failures
| Risk | Current or planned response |
|---|---|
| USB bridge hides SMART/ATA commands | report unavailable; never infer healthy |
| Disk identity/path changes after reconnect | per-handle serial/unique-ID revalidation before future raw operations |
| System disk selected for erase | planned hard exclusion in broker before raw operations can ship |
| Permission/UAC denied | current read engines report errors; future elevation isolated from UI |
| Image changes during reading | detect size changes and short read; same-size external modification is not fully detected, so use an immutable image |
| Power loss during copying | destination may remain partial after process/crash/power loss; only completed report + verified SHA establishes success; resume is not implemented |
| Counterfeit flash wraps existing allocations | explicit backup requirement before any write test; test-file isolation cannot protect data from dishonest firmware |
| Windows raw locks, 4Kn alignment, NVMe namespaces | dedicated future backend tests; no assumption of 512-byte sectors |
| CI lacks physical hardware | fault-injection tests and image fixtures only; physical success remains unverified |
| UI/build dependency failures | Linux and Windows CI, offscreen UI startup, portable packaging |
| Future Android request | outside current scope; raw disk access is not assumed to work through Android permissions |

## Persistence
SQLite stores the last 50 displayed records (database itself retains all records). Paths, serials and SMART output stay local. Export JSON is user-initiated. No telemetry or cloud service.
