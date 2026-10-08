# Technical research — 2026-10-07

Sources reviewed before implementation. These are candidates, not a claim that their engines are integrated. No third-party engine code is copied into this foundation.

| Candidate | Relevant capabilities | License shown by project | Platform / boundary |
|---|---|---|---|
| [openSeaChest](https://github.com/Seagate/openSeaChest) | identification, health, tests, supported firmware/erase operations; GenericTests and DST & Clean describe problem-sector treatment | MPL-2.0 | cross-platform; standard commands and approved Seagate vendor commands, not universal WD service-area access |
| [smartmontools](https://www.smartmontools.org/static/doxygen/) | SMART and self-test logs for ATA/SCSI/NVMe | GPL-2.0-or-later source headers | external optional smartctl invoked with fixed read-only arguments; not bundled |
| [OpenSuperClone](https://github.com/ISpillMyDrink/OpenSuperClone) | advanced clone, virtual disk, direct AHCI/IDE, relay control | GPLv2 | Linux; documentation incomplete, advanced integrations need dedicated validation |
| [GNU ddrescue](https://www.gnu.org/software/ddrescue/manual/ddrescue_manual.html) | failing-drive imaging with a mapfile and retries | review exact selected release before distribution | rescue first; operate on copy later |
| [TestDisk / PhotoRec](https://github.com/cgsecurity/testdisk) | partitions, filesystem recovery, carving | GPL v2 or later | not integrated; PhotoRec usually loses original names/tree |
| [nwipe](https://github.com/martijnvanbrummelen/nwipe) | overwrite, verification, reports | GPLv2 | Linux; erase is not physical repair |
| [nvme-cli](https://github.com/linux-nvme/nvme-cli) | NVMe health, sanitize and firmware commands | review exact selected release before distribution | Linux; hardware capability gating needed |
| [F3](https://github.com/AltraMayor/f3) | counterfeit-capacity tests; file tests and raw device probe | GPLv3 | f3write/f3read can be built for Windows; f3probe/f3fix/f3brew require Linux per README |

## Competitive reference
- [Victoria author FAQ](https://hdd.by/victoria_faq/): explains remap, rewrite/refresh, and risks to failing media. Archive documentation is not a current compatibility guarantee.
- [HDD Regenerator developer](https://www.dposoft.net/): claims magnetic error regeneration. This is a developer claim; no independent validation was performed, and no algorithm/source is supplied here.
- [PC-3000 Express manufacturer](https://www.acelab.eu.com/pc3000.Express.php): professional hardware + software, defect tables, firmware modules, some SelfScan, head disabling and damaged-area exclusion. No general open-source replacement established by this research.
- [Seagate zero-fill explanation](https://www.seagate.com/support/kb/how-do-i-low-level-format-a-sata-or-ata-ide-hard-drive-203931en/): firmware can retire defective sectors and activate spares during writes. Does not promise physical surface restoration.
- [Seagate firmware documentation](https://github.com/Seagate/openSeaChest/wiki/How-To-Update-Drive-Firmware): exact model/configuration matching.

## Distribution decision
Start with original code and optional, separately installed smartctl. Before bundling or modifying any engine, record its exact revision, full license, dependencies, notices and source-delivery obligations. Separate process execution alone is not treated as an automatic exemption from license obligations. Qt distribution obligations also require review before a commercial release. Firmware download rights must be checked separately from command-tool licensing.

## 0.4 native recovery implementation references
- Microsoft NTFS attribute records and mapping pairs: https://learn.microsoft.com/en-us/windows/win32/devnotes/attribute-record-header
- Microsoft NTFS record/fixup structure: https://learn.microsoft.com/en-us/windows/win32/devnotes/file-record-segment-header
- Microsoft exFAT specification: https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification
These are format references. No external recovery engine is bundled or linked; existing Qt/smartmontools redistribution notices are unchanged.
