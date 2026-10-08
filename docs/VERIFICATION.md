# Verification — 0.3.0

Verified code baseline: `a85a40539abf5a42acf484de8ec19502f06ebe3e`, GitHub Actions [37710044444](https://github.com/nexvary/NEXVARY-DC/actions/runs/37710044444), successful on Windows 2022 and Ubuntu 24.04. Windows passed 14 CTest suites; Linux passed 12. The UI is subsequently reordered to show file recovery first; release CI runs on every change.

Windows verification includes portable startup with an isolated runtime path, installer compilation, silent installation, installed startup and uninstallation. The hybrid-writer tests compile the actual C# helper, write a regular-file fixture using aligned sectors, check SHA-256 readback, verify source preservation and final-sector padding, and reject a changed source and a non-hybrid fixture. Read-only Windows storage inventory is also exercised. No test writes a physical disk.

Recovery tests check valid PNG CRCs, JPEG boundaries across scan chunks, corrupt/truncated candidates, cancellation preserving results, refusal of raw-device source paths, a sparse FAT32 image containing a deleted file and rejection once its cluster is allocated. Policy tests cover system/boot/read-only/offline protection and replacement-disk identities for all boot-media actions.

UI proof includes Arabic recovery, Arabic boot and English compact boot screenshots. Screenshot review prompted moving recovery controls and source selection to the top of the rescue page. The installer lists the supported modes and their limits.

Not hardware-verified: real SMART passthrough, failing HDD reads, external-media formatting/repair, raw hybrid ISO writing to a physical USB, BIOS/UEFI/Secure Boot execution, and recovered user files. Split WIM uses DISM integrity processing, not independent byte-for-byte readback of split WIM data. Windows writes are restricted to enumerated external USB/SD/MMC; Linux-hosted storage writes remain unsupported.

Recovery scope: regular image sources only, up to 64 MiB/file and 10000 files. FAT32 volume images and primary MBR partitions are supported; NTFS, exFAT, GPT recovery, extended partitions, original long names/folders and fragmented-file reconstruction are not. PNG chunks are CRC-checked; JPEG and FAT32 content remains a recovery candidate requiring inspection. Overwritten or TRIM-discarded original bytes cannot be reconstructed by these engines.
