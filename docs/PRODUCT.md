# NEXVARY Disk Care — Product contract

Independent desktop product in nexvary/NEXVARY-DC. Initial languages: Arabic (RTL) and English. Windows first, with a Linux workspace for engines that require it. C++20, Qt 6/QML, SQLite and CMake.

## Five workspaces
1. Disk diagnostics / sector treatment: physical identity, transport, SMART, read-error mapping; validated write treatment later.
2. Data rescue: image acquisition, recoverable-data prioritization, file recovery. Version 0.2 adds best-effort Windows physical imaging and read maps; resumable rescue and deleted-file recovery remain pending.
3. Maintenance / firmware: supported erase/sanitize, per-model compatibility, official firmware and before/after results. Version 0.2 adds filesystem scan/repair; firmware writes and physical sector treatment remain unsupported.
4. Flash drives / memory cards: filesystem diagnostics and repair, image rescue, write-protection analysis. Version 0.2 implements external-device filesystem operations and partition management on Windows.
5. Counterfeit capacity detection: report advertised bytes separately from verified test bytes. Never label sampled or filesystem-level verified bytes as exact physical NAND capacity.

## UX
Dark navy, silver borders, blue accents; restrained green/gold status cues. Original colored icons with labels and a branded bilingual installer. Back navigation, mirrored Arabic layout, scrolling for smaller windows, one active operation, cancellation, explicit write-test acknowledgement. Planned operations have explanatory pages rather than fake controls.

## Meaning of success
A successful read of an image says nothing about the original drive's physical health. A successful rewrite/remap does not prove new condition. SMART unknown is unknown, not healthy. Firmware is not a generic replacement image for all models. Do not reset health counters or conceal defect history.

## Phases / completion gates
- 0.1: inventory, external read-only SMART adapter, regular-image hashing/copy, bounded directory write/read verifier, reports, fault-injection tests and CI.
- 0.2 delivered scope: read-only surface scanning, Windows rescue image/read map, external storage management, Windows UEFI media, bundled SMART and redesigned UI. Resume validation, file recovery and comprehensive raw capacity remain outstanding.
- 0.3: F3-backed raw capacity probing in an offline Linux environment, verified compatibility; separate accurate capacity estimates from filesystem tests.
- 0.4: supported filesystem repair and recovery engines; preview and source-protection tests.
- 0.5: erase/sanitize and sector treatment for tested interfaces, system-disk lockout, exact device confirmation and reconnect identity checks.
- Specialist phase: vendor-specific firmware/service operations only when documented, supported and verified on sacrificial hardware. Never advertise universal PC-3000 equivalence.

Each phase requires passing automated tests, real-device evidence for hardware-dependent claims, UI inspection and an updated capability matrix.
