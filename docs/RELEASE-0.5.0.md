# NEXVARY Disk Care 0.5.0

Adds offline GRUB repair from x64 live Linux: validated mounted ext4 root,
explicit disk confirmation, revalidated identity/configuration, verified boot-file
backups, BIOS/MBR GRUB installation and UEFI installation/firmware boot order
preserving Windows entries. UEFI appends a Windows chainloader menu entry when
its loader exists. See BOOT-REPAIR-AR.md for supported layouts and usage.

Adds bounded NTFS file ATTRIBUTE_LIST extent recovery with sequence, base-record and VCN coverage checks. Adds CRC-validated 4Kn GPT primary/backup image recovery and deleted FAT32 folder
traversal only when retained chains and valid dot entries survive. Recovered FAT
contents remain candidates; no overwrite/TRIM recovery is claimed.

Windows Setup and Portable still include SMART and all existing recovery/rescue
features. The portable package also includes the live-Linux helper; Windows
itself can restore priority of an existing GRUB/shim UEFI entry with firmware/BCD backup and stale-plan checks; it does not reinstall GRUB. Download exact sizes/hashes and source commit
from release-manifest.json. Publication is gated on Linux/Windows builds, tests,
portable/installer lifecycle, reference hybrid Linux boots and repaired-disk
BIOS/UEFI kernel handoff tests for the same commit.

Not supported: encrypted/LVM/RAID/Btrfs Linux roots, separate /boot, BIOS GRUB on
GPT, missing/corrupt existing grub.cfg, Windows BCD rebuild, Secure Boot repair.
Full Windows installer/installed Windows firmware boot remains unverified.
NTFS compression/encryption/named streams/external MFT ATTRIBUTE_LIST remain outside
this release. Missing FAT32 directory chains cannot be reliably reconstructed.
