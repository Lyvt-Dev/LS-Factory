# LS Factory — native pattern workstation

LS Factory 0.2 is a working C++20/JUCE desktop foundation for the supplied DAW specification. It starts with an original synthesized demo session. The reference informs the dense slate workspace, left browser, top transport, coloured arrangement clips, floating step rack, piano keyboard and velocity lane. No reference audio, graphics, proprietary plugins or algorithms are included.

This is an implementation milestone, not the complete production DAW in the specification. See [implementation status](docs/STATUS.md) for the exact boundary.

## Build

Requirements: CMake 3.24+, Ninja, C++20 (Clang on Linux/macOS, MSVC on Windows), Git and zlib 1.2.9+. JUCE 8.0.15 and Catch2 3.8.1 are pinned to immutable commits and obtained automatically. Alternatively put JUCE in `libs/JUCE` or set the CMake `FETCHCONTENT_SOURCE_DIR_JUCE` and `FETCHCONTENT_SOURCE_DIR_CATCH2` cache variables.

On Debian/Ubuntu install the development packages for ALSA, X11/Xext/Xinerama/Xrandr/Xcursor, Freetype, Fontconfig and OpenGL. WebKit and curl are disabled for the app and are not required.

```sh
CC=clang CXX=clang++ cmake --preset debug
cmake --build --preset debug --parallel 3
ctest --preset debug
```

Run `build/debug/LSFactory_artefacts/Debug/LS Factory` on Linux. The corresponding Windows output is `LS Factory.exe`; macOS uses the app bundle. Windows builds require a Visual Studio developer shell with Ninja. Windows/macOS builds have not been executed in this cloud environment.

For core-only development without desktop/audio-device prerequisites:

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Cloud setup and activation:

```sh
bash tools/cloud-setup.sh
. /workspace/.tools/activate.sh
cmake --build --preset debug --parallel 3
ctest --preset debug
```

## Use

- Space: play/pause. Stop resets the position. SONG/PATTERN selects arrangement or selected-pattern playback.
- F5: playlist; F6: channel rack; F7: piano roll; F9: mixer.
- Select a pattern and channel in the toolbar. Click rack steps to toggle notes; click the channel LED to mute.
- Piano roll: click to draw, right-click to erase, drag to move, drag the right edge to resize. Click a velocity stem below the grid to change velocity. Wheel scrolls pitch; Ctrl/Command + wheel zooms horizontally.
- Ctrl/Command + Q quantizes the current channel's notes to sixteenths. Ctrl/Command + Up/Down transposes a semitone; add Shift for an octave.
- Playlist: click empty space on an existing track to place the selected pattern. Drag clips to move or drag their right edge to resize. Shift-click slices, Ctrl/Command-click duplicates, right-click erases.
- Click the automation lane to insert/update a point; right-click near a point erases it. Playback applies linear, step or smooth interpolation sample by sample.
- Mixer: click/drag faders; M/S mute/solo channels. Select a channel strip to edit filter, delay, pan, reverb, compression and saturation in the parameter panel.
- Drag WAV/AIFF/FLAC/OGG/MP3 files onto a rack channel to replace its instrument with a pitched sampler. Decoder availability follows JUCE's platform support. Samples are mono downmixed, limited to 60 seconds, linked by absolute path and loaded outside the callback.
- Ctrl/Command + Shift + N adds a synth channel, Shift + P adds a pattern, Shift + T adds a playlist track. The engine supports up to 64 channels and 64 simultaneous voices; the rack currently shows six channels at a time.
- Ctrl/Command + Z, Shift + Z or Y: undo/redo. Drags form a single transaction.
- SAVE/OPEN or Ctrl/Command + S/O use the compressed, checksummed `.odaw` format. Dirty sessions autosave every 30 seconds to the platform's application data directory under `LSFactory/recovery.odaw`; OPEN starts in that directory for manual recovery.
- INSERTS loads up to eight stereo VST3 master effects. A child process scans the selected bundle; the menu opens native/generic editors, bypasses or removes inserts. Projects persist opaque state and normalized parameter values. Instruments, per-channel inserts and CLAP are not implemented yet.
- EXPORT WAV renders the entire arrangement to 24-bit, 48 kHz stereo WAV on a worker.
- AUDIO configures audio inputs/outputs. REC IN writes an enabled input to stereo WAV using JUCE's buffered background writer. Stop recording before changing devices. The first available MIDI input is opened automatically; MIDI REC records into the selected pattern/channel and commits one undoable action when stopped.

## Headless validation

```sh
"build/debug/LSFactory_artefacts/Debug/LS Factory" --render-demo /absolute/path/demo.wav
"build/debug/LSFactory_artefacts/Debug/LS Factory" --export-json /absolute/path/demo.json
DISPLAY=:93 "build/debug/LSFactory_artefacts/Debug/LS Factory" --smoke-test /absolute/path/artifacts
```

The smoke command needs an X display on Linux. It exercises project edits and undo/redo, binary roundtrip, actual offscreen rendering of the three editors, synthetic input recording and WAV export. It returns a failing exit status on check failure. It does not prove a physical audio/MIDI device works. Debug JUCE may report the unavailable ALSA sequencer in machines without `/dev/snd/seq`.

## Design

`forge_core` has no JUCE dependency. It owns validated project data, snapshot-based command transactions, the bounded scheduler, synth/sampler DSP, automation and persistence. The UI publishes trivially copyable snapshots through an SPSC queue. The audio callback consumes them, splits blocks at loop boundaries, executes sample-offset events and renders fixed-pool voices. Sample buffers and effect delay lines are allocated while rendering is suspended. File decoding, saving and exporting use workers. Device MIDI and capture events use separate SPSC queues. Tempo maps can be represented and integrated, but realtime playback currently accepts a single fixed tempo.

Snapshot capacity errors are explicit. The current arrangement limit is 1024 expanded notes, 16 automation lanes with 64 points each and a 100-entry undo history. Snapshot changes reset voices; seamless graph transitions and sample chasing are future work. The oscillator has PolyBLEP saw/square waveforms; the drum sounds are generated rather than shipped samples.

## Licensing

Original repository code is MIT-licensed. JUCE remains under its own AGPLv3/commercial licence; Catch2 is BSL-1.0 and zlib has its own licence. Review the chosen JUCE licence for distribution. See [third-party notices](docs/THIRD_PARTY.md).

## Delivery and milestone releases

Pushes to `main` run Linux, Windows and macOS native release builds and tests and upload downloadable application archives as GitHub Actions artifacts. Version tags `v*` run the same validation and publish a GitHub prerelease only after all build jobs pass. Release archives include the original PNG screenshots, dependency notices, commit/version metadata and SHA-256 checksum files. This desktop workflow distributes native applications; there is no web service to deploy.

The workstation milestone is `v0.2.0`; installer delivery is `v0.2.2` (the `v0.2.1` Windows startup check failed, so that tag was not released). Later large milestones increment the minor version; fixes increment the patch. Never move a published tag. If a tagged build fails, fix the failure and publish a new version. The workflow's GitHub token is scoped to read contents for builds and write contents only for the release job.

VST3 integration validation uses an actual test-only shared-library plugin:

```sh
xvfb-run -a "build/debug/LSFactory_artefacts/Debug/LS Factory" --plugin-smoke-test "$PWD/build/debug/ForgeGainFixture_artefacts/Debug/VST3/Forge Gain Fixture.vst3" "$PWD/build/plugin-smoke"
```

Project format v4 loads existing v3 sessions. Unknown versions and truncated/oversized plugin payloads fail before the active project is replaced. Plugin loading and state capture suspend the audio callback. Scanning is process-isolated; playback plugins run in the app process and are not crash-isolated. Host buffers are preallocated; third-party plugin callback behaviour remains outside the host's control. Plugin editors' parameter changes are persisted but are not yet part of the application's undo history.

## Install from a release

Windows: download `LSFactory-<version>-windows-x64-Setup.exe`, run the wizard and launch LS Factory from the Start menu. Installation defaults to the current user's Programs directory and includes an uninstaller. Desktop shortcuts and `.odaw` project associations are optional.

Linux: download `LSFactory-<version>-linux-x64-Installer.AppImage`, make it executable and open it. This AppImage starts an installation wizard. The wizard verifies and installs the embedded application into `~/.local/opt/lsfactory` by default, writes a launcher to `~/.local/bin/lsfactory` and creates an application-menu entry. You can choose another folder. Existing user files in a managed installation are preserved. The generated `uninstall.sh` removes managed app files and leaves personal projects/settings intact.

If the system cannot mount AppImages, start the installer with `--appimage-extract-and-run`. The installed launcher uses extraction mode automatically and does not need a FUSE mount. Releases are development prereleases and are unsigned. Verify the corresponding SHA-256 checksum. The packaging workflow installs both installers into temporary locations and starts the resulting application before publishing.
