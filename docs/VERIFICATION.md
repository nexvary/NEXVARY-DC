# Verification — 0.2.0

Local Linux release build and expanded test suites are being checked. Windows CI and installer verification are pending; this document will be updated from completed results.

Policy tests cover stable disk identity, system/boot/offline/read-only rejection, external-only writes, partition offset/size replacement, filesystem constraints and health unknown/failed distinction. Imaging tests use file fixtures for valid data, unreadable chunks, zero fill/read map, independent readback, refusal to overwrite, cancellation and source preservation. These tests do not constitute physical-device validation.

Hardware not yet tested: real SMART passthrough, bad HDD reads, USB/card filesystem mutation, Windows setup boot, card-reader firmware and physical counterfeit capacity. Split WIM processing uses DISM integrity checking but is not byte-for-byte destination readback of the split WIM. Installation/uninstallation will be exercised on the Windows runner, not on a user's physical machine.
