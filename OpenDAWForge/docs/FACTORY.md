# Factory library and idea engine

The additions are implemented as independent modules under `src/library`: `Library` owns the catalog, filtering, local state, pack validation and deterministic idea generation; `LibraryPanel` provides search, metadata filters, auditioning and the macro preset editor; `LoopStarter` provides genre/BPM/key/mood controls and per-part replacement. `Workspace` bridges these modules to existing transactional project commands and internal drag/drop. `AudioEngine` prepares a separate audition renderer on the workspace worker and swaps it at audio block boundaries. It never changes the project's notes or transport to audition library content.

Factory assets are original, generated PCM audio and native instrument presets under `resources/factory`, reproduced by `tools/generate-factory.py`. The 46 Essentials entries are embedded through JUCE BinaryData; no separate network download or library service is required. They include presets, samples, one-shots and four-beat drum loops, with Drums, Bass, Synths, Keys, Pads, FX and synthetic vocal textures. Vocals are synthesized textures, not human recordings. Factory content is CC0; application and dependencies retain their own licenses.

## User workflow

Use **FACTORY** to search and filter by type, category, genre, instrument, mood, BPM or comma-separated tags. BPM can also be typed for imported content. **Preview** or double-click auditions the selected sound before loading. The macro controls edit volume, pan, filter, delay, reverb, compression and drive; **Save preset** writes a new user preset without modifying the factory definition. Sounds can be marked as favourites, accessed in Recent, or added to named collections.

**Load to project** offers a new instrument track, replacement of the selected channel, or addition to the current pattern. Drag a sound onto a Rack channel to replace its sound, or onto the Playlist to create a clip. Audio loops use sampler-triggering pattern clips; this does not add a separate audio-clip model or a time-stretch algorithm. Match the project BPM to the displayed loop BPM for the original timing. Existing project content and tempo are retained unless an explicit control requests a change.

**LOOP STARTER** creates an eight-beat MIDI idea from genre, BPM, key, major/minor mode and mood. Generate creates a new seed; Replace changes the selected part's performance and sound. Preview idea uses a separate renderer; Preview selected auditions only the chosen sound. Add to pattern creates a combined pattern and its instruments; Add to arrangement creates separate patterns/tracks. **Adopt this BPM** is optional and initially off. Previous tracks, patterns, plugin state and sounds are preserved. Recording must be stopped before loading structural factory content.

## Storage and compatibility

The existing Channel, Pattern, Track and PatternClip model and `.odaw` format v4 remain in use, including v3 migration. Presets use existing native parameter fields. No existing project or preferences schema is replaced.

Library state and user presets live in the platform's application-data directory under `LSFactory/library`. Audio is verified and copied to content-addressed permanent cache files, so a project never points into a temporary AppImage mount. Updates retain this directory and old cached audio. State export/import merges favourites, recent IDs and collections instead of deleting existing collections. Preset saves are transactional. Import rejects unsupported entries, unsafe paths, symlinked pack audio and checksum mismatches. Limits: 1,024 entries, 256 entries per imported pack, 64 MiB per audio file, 512 MiB per pack, 60 seconds per decoded sample and the existing project/engine capacities.

## Optional packs, premium content and cloud

The Packs / downloads page separates bundled free factory content from user packs and optional premium content. No shop/provider is configured; no purchase, premium download, entitlement or synchronization is claimed. Premium pack imports require a provider and are rejected in this version. Local favourites/collection transfer and free pack import work independently of any account.

`CloudProvider` is the extension seam for a catalog, pack download, entitlement validation and synchronization of library state. A real integration must supply a configured HTTPS service, authentication/secure credential storage, trusted pack manifests and server-validated entitlements. No credentials or license tokens are placed in project files or library-state exports. A future provider must only synchronize after the user opts in, validate content before installation and preserve local user data on conflicts/failure.

## Pack format

Select a schema-1 JSON manifest with its audio files next to it. Paths are single relative filenames. An example entry is:

```json
{
  "schema": 1,
  "pack": "My Free Pack",
  "entries": [{
    "id": "my-kick", "name": "My Kick", "kind": "one-shot",
    "category": "Drums", "genre": "House", "instrument": "Kick",
    "mood": "Energetic", "bpm": 0, "beats": 4, "tags": ["kick", "soft"],
    "license": "CC0-1.0", "tier": "free", "file": "kick.wav",
    "sha256": "REPLACE_WITH_64_LOWERCASE_HEX_DIGITS",
    "parameters": {"type": 4, "volume": 0.7, "pan": 0, "cutoff": 1,
      "delay": 0, "reverb": 0, "compression": 0, "drive": 0}
  }]
}
```

Type values are the existing engine enum: 0 synth, 1 kick, 2 snare, 3 hat, 4 sampler. Kind is preset/sample/one-shot/loop. Audio-loop entries require BPM and can supply their length in beats (default 4). Imported audio must be supported by JUCE's decoders. Keep its real audio file extension.

## Verification

`--factory-smoke-test <directory>` verifies every embedded audio checksum, all categories, metadata filters, favourite/recent/collection persistence and transfer, user preset reload, deterministic/key-correct generation, pattern/arrangement insertion, existing-project preservation, `.odaw` roundtrip, safe free-pack import, traversal rejection and premium rejection without a provider. The GUI smoke additionally tests actual project drag/drop and idea insertion with Undo, audible isolated previews with unchanged song position, and the library/idea/settings UI. Existing core, migration and actual VST3 tests remain enabled.
