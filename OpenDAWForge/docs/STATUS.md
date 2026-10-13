# Implementation boundary — 0.4.4

The full request describes a substantial multi-release DAW. This repository implements the initial native workstation rather than asserting that every requested subsystem is complete.

| Area | Implemented | Still required |
|---|---|---|
| Build | C++20, JUCE 8.0.15, CMake presets, Clang Linux build, Catch2 | Further platform coverage, ASIO option |
| Core/model | Strong IDs, results, time conversion, tempo integration, validated channels/patterns/clips/automation | Audio/group clip models, routing graph, markers/time-signature maps |
| Commands | Validated transactional edits, 100-entry undo/redo, single-command drags | Dedicated per-domain command objects, large-project memory optimisation |
| Engine | Fixed 64-voice pool, sample-offset scheduling, arbitrary-block loops, bounded snapshots, native render and meters | Graph crossfades, recorded-event sub-block timestamps, comprehensive realtime stress validation |
| Transport | Play, pause, stop, fixed BPM, song/pattern mode, looping | Metronome, count-in, punch, preroll, realtime tempo/time-signature changes |
| Synth/drums | PolyBLEP saw + sine voices, ADSR, low-pass, generated kick/snare/hat, voice stealing | Three-oscillator editor, wavetables, modulation matrix, unison, glide, dedicated pad UI |
| Sampler | Worker decoding, mono downmix, linear pitch interpolation, reverse model flag, linked sample persistence | Stereo voices, loop points, slices, multisamples, velocity layers, round robin, stretch |
| Rack | Movable/resizable rack, scrolling channels, 16 clickable steps, mute, current-channel selection, sample file drop | Arbitrary pattern step pages, channel rename/delete/copy UI |
| Piano roll | Add/delete/move/resize, ghost notes, isolated key/note audition, velocity editing, pitch scroll/zoom, quantize, transpose | Multiselection, clipboard, chords/strum/arp/humanize/legato, drum and step-input modes |
| Playlist | Pattern clips, add/move/resize/delete/duplicate/slice, grid zoom, loop indicator, automation point editing | Audio clips/waveforms/fades, clip offsets preserving sliced pattern phase, track scroll, markers |
| Mixer | Master + channel gain, equal-power pan, mute/solo, real peak meters | Full 64-strip navigation, sends/returns/groups/sidechain routing, per-channel inserts, PDC, RMS/loudness |
| Effects | Channel low-pass, delay, algorithmic reverb, peak compressor, saturation; independently tested 8-band EQ | EQ UI/insert integration, limiter with lookahead, chorus/phaser/flanger/stereo widener, sync/ping-pong delay |
| Automation | Volume/pan/filter/delay/reverb, sample-by-sample linear/step/smooth interpolation, project roundtrip | Recording, plugin/tempo targets, editable curvature, relative lanes |
| MIDI | First input, note playback, loop-note capture and commit, quantization via note editor | Multiple devices, MIDI output/thru, CC/bend/program/bank handling, MPE/clock |
| Recording | Background 24-bit WAV input writer, bounded queue/drop counter; synthetic input smoke | Physical-device validation, recorded audio clips in arrangement, monitoring/punch |
| Plugins | Process-isolated VST3 scanner, eight stereo master effects, native/generic editors, bypass, opaque state + normalized parameter roundtrip, tested real VST3 fixture | VST3 instruments, CLAP/AU user workflows, per-channel inserts, parameter automation, playback crash isolation, PDC |
| Persistence | Versioned compressed binary, checksum, bounded transactional reader, JSON debug export, autosave/manual recovery, plugin state, v3→v4 migration | Automatic recovery prompt, sample embedding, future migrations |
| UI | App-drawn window controls/fullscreen, draggable/resizable workspace panels, empty sessions and instrument-track creation, sixteen-chapter German tutorial, browser filtering, shortcuts, device dialog | Modular panel classes as specified, docking, full context menus/tooling, browser filesystem tree, explicit scale control |

There are no FL Studio original plugins, samples, graphics or proprietary algorithms. VST3 stereo effect hosting is verified with the shipped test fixture. Physical-device behaviour and arbitrary third-party plugin compatibility are not claimed. Instruments are still native synth/sample based.

The next concrete implementation step is per-channel insert routing and latency compensation, followed by CLAP hosting and plugin parameter automation. The eight-insert master chain is implemented and validated; it does not establish PDC or instrument hosting. Before a production release, the initial editor shell should be split into dedicated Components and every remaining row validated on the target platforms.

Installer delivery: native Linux wizard packaged as a genuine AppImage containing the application payload; Inno Setup on Windows. Packaging tests check actual installation and startup, with Linux checksum/update/user-file preservation checks.

Installer regression coverage includes the actual wizard button/worker, changed working directory, paths with spaces and an outer-AppImage argv[0]. GUI smoke checks empty startup, no preloaded notes, adding/undoing an instrument track, rack note editing, panel move/resize, per-view layout and fullscreen toggling. FUSE mounting and physical window-manager interactions require validation on the target desktop; the cloud checks extracted AppImages and JUCE window state.

Factory 0.4: 46 original embedded entries, full metadata filtering, favourites/recents/collections and merged state transfer, macro preset editing/saving, verified free-pack import, isolated audition, deterministic key/genre/mood-aware eight-beat ideas and undoable pattern/arrangement/drag-drop integration. Original v4 projects and v3 migration are retained. Premium/cloud has an extension interface only because no service is configured; audio time stretch and a separate audio-clip model remain outside this addition.

Application updater: official verified HTTPS feed, numeric version ordering, bounded hash/size-verified downloads, persistent manual/automatic settings and Windows/Linux wait/install/restart. Automatic installation is initially off and respects saved/idle/editor guards. Mac remains manual. Physical-device/desktop observations are distinct from offline audio-render and CI installer validation.
