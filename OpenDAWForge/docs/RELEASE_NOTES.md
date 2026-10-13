LS Factory 0.2.2 is a native desktop development milestone.

- Reference-inspired playlist, step rack, piano roll and mixer with original synthesis and sample playback.
- Isolated VST3 effect scanning, up to eight stereo master inserts, native/generic plugin editors and bypass.
- Opaque plugin-state and normalized parameter persistence in compressed project format v4, with migration of existing v3 sessions.
- Actual VST3 fixture tests: gain processing, two-insert order, bypass, state restoration and transactional failed replacement.
- Fixed Windows startup and export stack overflow by allocating large temporary audio engines on the heap; installed-app GUI/WAV smoke tests now gate Windows packaging.
- Windows Setup.exe with per-user installation, uninstaller and optional shortcuts/project association.
- Linux desktop shortcuts use the system desktop directory and are removed by the uninstaller without deleting user files.
- Linux Installer.AppImage with a native installation wizard, checksum-verified embedded program payload, user installation, menu entry, launcher and update preservation checks.
- Native build artifacts and downloadable PNG screenshots. Each archive includes commit/version metadata, dependency notices and a SHA-256 checksum.

This is a prerelease. VST3 instruments, CLAP hosting, per-channel inserts, PDC, advanced routing and the remainder of the full DAW specification are still under development. Windows/macOS artifacts are published only if their CI build and core tests succeed. Physical audio/MIDI devices and arbitrary third-party plugins need validation on your own machine. Linux CI binaries target Ubuntu 24.04 or a compatible newer runtime. The macOS build is native to the selected runner, not a universal or notarized bundle. Windows binaries are unsigned. The Linux AppImage is the installer, not merely a portable application launcher.

The source for this exact version is available from the release tag. JUCE and other dependencies retain their own licences; see the bundled third-party notices. The test fixture is not a production plugin and is not included in the app archive.
