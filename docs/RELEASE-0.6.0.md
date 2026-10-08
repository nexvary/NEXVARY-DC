# NEXVARY Disk Care 0.6.0

Adds selective rescue retry: validates a completed previous image and original JSONL
map before any failed-media reads, preserves the original and copies healthy image
sectors into a separate output. Only previously unreadable sectors are read from the
source. Cancellation/resume, source/output identity and SHA-256 readback remain checked.
Requires space for a second full image. Physical-source support is currently Windows.
Does not control hardware read timeout, controller reset or drive power.

Adds evidence-based SMART connection/model/sector capability reports, fault-indicator
recommendations and explicit lack of certified Firmware/Service Area/Translator repair
profiles. Observed bridge or Western Digital model is not proof of vendor command support.

Linux CI qualifies an unmodified, separately built OpenSuperClone stable v2.5.0 source
at 87a25d257e44337ae15f863e59b0062311a5c329. GPLv2 review, build/help/version and source/license
retention are provided; no driver installation, disk commands, relay or firmware repair
is enabled. This is engine qualification, not an integrated PC-3000 replacement.

Preserves 0.5 boot repair, NTFS/exFAT/FAT32 recovery, GPT/MBR parsing, rescue, SMART,
Arabic/English UI and Windows installer/portable lifecycle. Reports now use the actual
application version. VM test provisioning includes the required UTF-8 filesystem module.

Publication requires Windows/Linux checks and reference/repaired BIOS+UEFI VM tests
on this exact commit. Manifest includes byte sizes/SHA-256. Full Windows installer boot
and Secure Boot remain unverified. Unsupported filesystem/GRUB layouts are documented in
VERIFICATION.md. No physical repair, overwritten/TRIM reversal or generic firmware update.
