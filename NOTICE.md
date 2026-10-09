# Distribution notices

NEXVARY Disk Care original application code and icon assets, 2026. Developer identity/contact values were reviewed in FG MTM Android and Windows sources.

The Windows distribution includes the unmodified smartctl executable from smartmontools 7.5. GPL v2 notice and the corresponding upstream source archive are included under licenses/smartmontools. The packaging script verifies the official Windows installer SHA-256 recorded in microsoft/winget-pkgs before extraction. smartctl runs as a separate process; its source is not linked into the application.

Qt 6.8.3 runtime libraries are dynamically deployed. LGPL v3 / GPL v3 license text is included under licenses/Qt. Users may replace the compatible Qt runtime libraries and reverse engineer for debugging modifications to these libraries. Corresponding Qt 6.8.3 source is available at https://download.qt.io/archive/qt/6.8/6.8.3/ (the source subdirectory). No Qt modifications are shipped. Review each module/plugin's notices before commercial distribution.

App-local Microsoft Visual C++ runtime DLLs are redistributed from the MSVC redist folder under Microsoft's redistribution terms.

Arabic Inno Setup translation comes from jrsoftware/issrc Files/Languages/Arabic.isl; translator attribution is retained in that file. Other-company names identify compatibility and upstream tools; they do not imply endorsement.

Offline Linux boot repair invokes the live distribution's installed GNU GRUB and efibootmgr tools. No GRUB binaries are bundled in the Windows installer or portable archive; obtain those tools and their license notices from your Linux distribution. The bundled recovery helper uses the Python standard library.

## Restricted OpenSuperClone adapter (0.7.0)
The separate Linux adapter executes OpenSuperClone stable v2.5.0, commit 87a25d257e44337ae15f863e59b0062311a5c329, GPL-2.0-or-later, as an external process. Modified C sources are distributed as upstream source archive plus adapter.patch and LICENSE; they are not linked into the Qt executable. Build script retains the exact patch and binary SHA-256. Controller/port selection modifications carry the same GPL terms. USBRelay protocol reference is upstream usbrelay.c/usbrelay.h; no generic firmware scripts are exposed.

## CrystalDiskInfo advanced engine (development toward 0.8.0)
Windows packaging pins and preserves the complete unmodified CrystalDiskInfo 9.9.2 Standard Edition portable package. The MIT-licensed core and every accompanying dependency notice remain attributed under licenses/CrystalDiskInfo and engines/crystaldiskinfo/License. The corresponding original source ZIP is included. AMD_RC2t7 has its own redistribution terms and is not represented as MIT/open-source; its unmodified version/hash/license declarations are preserved, and no charge is made for its functions/results. The original advanced panel opens in a separate, explicitly identified window. It is not presented as a native Qt reimplementation or universal controller support. Automatic saved AAM/APM application is disabled at launch. Linux uses smartctl, not this Windows engine.
