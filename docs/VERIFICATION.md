# Verification — 0.1.0 foundation

Verified implementation commit: `fca34b99dd1b9484f616e55cadb737c64d65deb8`.

## Build and automated checks

[GitHub Actions run 37586420209](https://github.com/nexvary/NEXVARY-DC/actions/runs/37586420209) completed successfully on Ubuntu 24.04 and Windows Server 2022 on 2026-10-07. Both platforms passed all five CTest targets: core, controller, overview startup, compact Arabic capacity page and compact English capacity page.

The core suite exercises eight substantive cases, including injected wrapped-address media, single-byte corruption, forced write failure, cancellation, exclusive image copy and SHA-256 destination readback. A temporary-directory test verified 1 MiB, retained an existing user file and removed the application's test files. Controller tests exercise async jobs, single-job exclusion, history and export. Linux also tests cancellation of slow disk discovery. Screenshots were captured from the running executable and visually inspected; the UI smoke tests reported no QML warnings.

## Windows delivery

The same run successfully deployed Qt plugins and app-local Visual C++ runtime DLLs, passed portable startup with PATH restricted to Windows system directories and Qt plugin/import environment variables cleared, and built the Inno Setup installer. This verifies packaged application startup; installing/uninstalling the installer on a physical Windows machine has not been tested.

Installer: `NEXVARY-DC-0.1.0-Setup.exe`, 29,459,682 bytes.
SHA-256: `01edc7fbe62e92f92a6239b3244f24c2e06d475fb17972518520a43bdd03eebf`.
Installer artifact ZIP SHA-256 was checked against GitHub's artifact digest before extraction. The extracted executable's MZ/PE signatures were also checked.

The earlier Windows smoke-test stall was resolved by explicitly exiting the smoke run instead of invoking the interactive close guard. Missing offscreen plugin and runtime deployment dependencies were fixed and the isolated startup check passed. Tests have 45-second limits; CI jobs have a 15-minute limit.

## Scope and remaining verification

This is a working experimental foundation with original file-operation code. Directory capacity tests verify only the bytes allocated and tested through the filesystem; they do not determine exact physical NAND capacity. Mismatches can indicate counterfeit storage or a fault.

Physical SMART reads, USB/card readers, physically failing HDDs, real counterfeit media and vendor firmware have not been validated on hardware. No raw erase, remap, firmware update, partition repair or exact original-capacity detection is shipped. Physical surface regeneration is not claimed.

The Linux build uses Qt 6.8.3 and GCC 13. Local container testing previously needed an environment-specific OpenGL override; CI installs normal development packages.
