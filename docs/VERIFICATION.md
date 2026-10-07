# Verification — 0.1.0 foundation

Status: Linux Release build passed. All five CTest targets passed (core, controller, overview startup, compact Arabic capacity page, compact English capacity page). Windows CI is pending.

The core suite passed eight substantive test cases, including injected wrapped-address media, single-byte corruption and forced write failure. A real temporary-directory test verified 1 MiB, retained an existing user file and removed all application test files. UI screenshots were captured from the running executable and inspected. No QML warnings occurred in the startup smoke tests.

Scope: real executable + original file-operation core, not hardware repair claims. Automated tests exercise wrapped fake media, corruption, write failure, cancellation, exclusive copy, SHA-256 readback, test-file cleanup and preservation of an existing user file. Controller tests exercise async jobs, single-job exclusion, history and export. UI smoke runs include Arabic and English capacity workspaces at the minimum window size.

Hardware not validated: physical SMART reads, USB/card readers, physically failing HDDs, real counterfeit media, vendor firmware or raw repair. Exact original capacity detection is not shipped. No raw erase, remap, firmware update or partition repair is shipped.

The Linux build uses Qt 6.8.3 and GCC 13; an environment-specific OpenGL library override is necessary in this container because development headers are unavailable. CI installs normal development packages.
