# User observations — 0.2.0 implementation

1. UI: compact storage dashboard, selected disk context, connected-disk rows and denser action cards. Actual rendered screenshots required before delivery.
2. SMART: pinned bundled engine; readable health summary; raw JSON optional. Hardware readings remain unverified.
3. Previously unavailable functions: filesystem management and read-only surface/rescue are implemented. Universal firmware flashing and physical regeneration cannot be enabled as generic commands.
4. Developer: Alaa Mohamed, company and developer links reused from current FG MTM Android/Windows developer sources. Product identity is NEXVARY.
5. Agreed scope: real disk diagnostics, imaging, filesystem maintenance, flash/card tests and OS-media workflow advanced. Version 0.3 adds image-based PNG/JPEG and contiguous FAT32 recovery candidates, Windows BIOS/UEFI media and hybrid Linux ISO writing. Exact raw capacity, firmware profiles, NTFS/exFAT recovery and resumable rescue remain outstanding; no claim that all scope is finished.
6. Original colored vector/raster icons, organized cards and destructive-action warnings.
7. Windows external USB/SD/MMC format, create/delete partitions and destructive GPT/MBR layout creation. Windows x64 UEFI ISO preparation; unsupported boot modes explicitly stated.
8. Every storage management action requires review, typed device path and backup acknowledgement. Single-use token expires after 180 seconds. Execution checks fresh hardware identity and partition offsets/sizes. Internal/system/pagefile/source disks are protected. No application password is needed because the user accepted confirmation as an alternative.
9. Branded icon on EXE/installer/shortcuts, Arabic/English modern wizard, feature/scope page, desktop shortcut, postinstall launch and uninstaller.

## Research grounding
- https://learn.microsoft.com/en-us/powershell/module/storage/format-volume
- https://learn.microsoft.com/en-us/powershell/module/storage/new-partition
- https://learn.microsoft.com/en-us/powershell/module/storage/clear-disk
- https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/install-windows-from-a-usb-flash-drive
- https://github.com/smartmontools/smartmontools/releases/tag/RELEASE_7_5
- https://github.com/microsoft/winget-pkgs/blob/master/manifests/s/smartmontools/smartmontools/7.5/smartmontools.smartmontools.installer.yaml

## Risk boundaries
Physical writes are Windows-only and restricted to external media. Shared Windows storage services do not create a universal transactional rollback for partition changes. Disk replacement, source location, stable identity and pagefile/system flags are rechecked immediately before commands; disconnection during mutation is still hazardous. Storage cmdlets cannot safely be force-cancelled mid-operation, so the application waits for completion and blocks close.

The program does not promise to repair a physically bad sector. Readable sectors can be copied; unreadable ranges are explicit. Filesystem repair changes logical metadata and is distinct from repairing physical media.
