# Distribution notices

NEXVARY Disk Care original application code and icon assets, 2026. Developer identity/contact values were reviewed in FG MTM Android and Windows sources.

The Windows distribution includes the unmodified smartctl executable from smartmontools 7.5. GPL v2 notice and the corresponding upstream source archive are included under licenses/smartmontools. The packaging script verifies the official Windows installer SHA-256 recorded in microsoft/winget-pkgs before extraction. smartctl runs as a separate process; its source is not linked into the application.

Qt 6.8.3 runtime libraries are dynamically deployed. LGPL v3 / GPL v3 license text is included under licenses/Qt. Users may replace the compatible Qt runtime libraries and reverse engineer for debugging modifications to these libraries. Corresponding Qt 6.8.3 source is available at https://download.qt.io/archive/qt/6.8/6.8.3/ (the source subdirectory). No Qt modifications are shipped. Review each module/plugin's notices before commercial distribution.

App-local Microsoft Visual C++ runtime DLLs are redistributed from the MSVC redist folder under Microsoft's redistribution terms.

Arabic Inno Setup translation comes from jrsoftware/issrc Files/Languages/Arabic.isl; translator attribution is retained in that file. Other-company names identify compatibility and upstream tools; they do not imply endorsement.

Offline Linux boot repair invokes the live distribution's installed GNU GRUB and efibootmgr tools. No GRUB binaries are bundled in the Windows installer or portable archive; obtain those tools and their license notices from your Linux distribution. The bundled recovery helper uses the Python standard library.
