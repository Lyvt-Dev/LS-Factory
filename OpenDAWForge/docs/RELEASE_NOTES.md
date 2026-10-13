LS Factory 0.4.3 includes explicit Windows GUI-process waiting and visible-key/screen-size-independent GUI checks, explicit async drag/drop coverage and readable smoke-test failure diagnostics in the release pipeline.

This milestone adds the modular factory workspace, note auditioning and optional automatic application updates.

- Factory Library with 46 original CC0 presets, samples, loops and one-shots; type/category/genre/instrument/mood/BPM/tag filters, favourites, recents and local collections.
- Native preset editor with reusable user presets, permanent verified audio caching and safe free-pack import. Existing .odaw v4 projects and v3 migration remain compatible.
- Loop Starter with genre/BPM/key/mood controls, randomized eight-beat ideas, per-part replacement, isolated sound/idea preview and undoable pattern/arrangement insertion. Existing content and tempo remain unless explicitly changed.
- Drag factory sounds into the Rack or Playlist. Audio loops use the existing sampler/pattern-clip engine; no separate audio-clip model or pitch-preserving time stretch is claimed.
- Piano-key clicks, note placement and note selection audition the selected instrument briefly without starting the song or duplicating MIDI recordings. Preview releases leave playing voices of the same pitch intact.
- SETTINGS offers verified official-release downloads, manual installation and optional automatic checking/installing, with unsaved-project/recording/editor guards, wait-for-exit and restart on Windows/Linux.
- Optional cloud/provider seam and clearly separate premium area; no provider is connected. Local state transfer works; premium downloads, purchases, licensing and cloud synchronization remain inactive until a real service is integrated.
- Installer AppImage and Windows Setup.exe with tested install/update/startup/uninstall, checksums, notices and original UI screenshots.

See docs/FACTORY.md and docs/UPDATES.md for workflow, storage, limits and tests. This is a development prerelease. The existing implementation boundary remains: VST3 instruments, CLAP, per-channel inserts, PDC, advanced routing and a complete FL Studio-equivalent DAW remain unfinished. Audio loops should match project BPM for original timing. Windows binaries are unsigned; Linux binaries target Ubuntu 24.04 or a compatible runtime, and the Mac native ZIP is not universal/notarized and requires manual installation. Physical audio/MIDI hardware, actual desktop/FUSE behavior and arbitrary third-party plugins require validation on the target machine.

No FL Studio sounds, presets, plugins, proprietary graphics or algorithms are distributed. Factory content is original and CC0; application/dependency licenses are bundled separately.
