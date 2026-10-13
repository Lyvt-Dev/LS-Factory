#pragma once
#include "core/IDs.h"
#include "core/Result.h"
#include <cstdint>
#include <string>
#include <vector>
namespace forge
{
enum class InstrumentType : std::uint8_t
{
    Synth,
    Kick,
    Snare,
    Hat,
    Sampler
};
struct MidiNote
{
    int channel = 0, pitch = 60;
    double startBeat = 0, lengthBeats = 0.25;
    float velocity = 0.8f;
    bool operator==(const MidiNote &) const = default;
};
struct AutomationPoint
{
    double beat = 0;
    float value = 0;
    bool operator==(const AutomationPoint &) const = default;
};
struct Pattern
{
    PatternID id;
    std::string name = "Pattern 1";
    std::uint32_t color = 0xff99bf79;
    double lengthBeats = 4;
    std::vector<MidiNote> notes;
    bool operator==(const Pattern &) const = default;
};
struct Channel
{
    ChannelID id;
    std::string name;
    std::uint32_t color = 0xff99bf79;
    InstrumentType instrument = InstrumentType::Synth;
    float volume = 0.65f, pan = 0, cutoff = 0.6f, delay = 0;
    std::string samplePath;
    bool sampleReversed = false;
    float reverb = 0, compression = 0, drive = 0;
    bool muted = false, soloed = false;
    bool operator==(const Channel &) const = default;
};
struct PatternClip
{
    ClipID id;
    PatternID patternId;
    double startBeat = 0, lengthBeats = 4;
    int transpositionSemitones = 0;
    bool muted = false;
    bool operator==(const PatternClip &) const = default;
};
struct Track
{
    TrackID id;
    std::string name;
    std::uint32_t color = 0xff99bf79;
    std::vector<PatternClip> clips;
    bool operator==(const Track &) const = default;
};
struct TempoPoint
{
    double beat = 0, bpm = 120;
    bool operator==(const TempoPoint &) const = default;
};
class TempoMap
{
  public:
    std::vector<TempoPoint> points{{0, 120}};
    [[nodiscard]] double bpmAt(double beat) const noexcept;
    [[nodiscard]] double secondsAt(double beat) const noexcept;
    bool operator==(const TempoMap &) const = default;
};
struct ProjectMetadata
{
    std::string name = "Untitled Project", author, comments;
    bool operator==(const ProjectMetadata &) const = default;
};
enum class AutomationTarget : std::uint8_t
{
    Volume,
    Pan,
    Cutoff,
    Delay,
    Reverb
};
enum class Interpolation : std::uint8_t
{
    Linear,
    Step,
    Smooth
};
struct AutomationLane
{
    TrackID id;
    std::string name;
    int channel = 0;
    AutomationTarget target = AutomationTarget::Volume;
    Interpolation interpolation = Interpolation::Linear;
    std::vector<AutomationPoint> points;
    bool operator==(const AutomationLane &) const = default;
};
struct PluginSlot
{
    PluginInstanceID id;
    std::string name, descriptionXml;
    std::vector<std::uint8_t> state;
    bool bypassed = false;
    std::vector<float> normalizedParameters;
    bool operator==(const PluginSlot &) const = default;
};
struct Project
{
    ProjectMetadata metadata;
    TempoMap tempoMap;
    std::vector<Channel> channels;
    std::vector<Pattern> patterns;
    std::vector<Track> tracks;
    std::vector<AutomationLane> automation;
    std::vector<PluginSlot> masterInserts;
    double loopStart = 0, loopEnd = 16;
    bool loopEnabled = true;
    float masterVolume = 0.8f;
    [[nodiscard]] Channel *findChannel(ChannelID id) noexcept;
    [[nodiscard]] const Channel *findChannel(ChannelID id) const noexcept;
    [[nodiscard]] Pattern *findPattern(PatternID id) noexcept;
    [[nodiscard]] const Pattern *findPattern(PatternID id) const noexcept;
    [[nodiscard]] Result validate() const;
    [[nodiscard]] std::uint64_t nextId() const noexcept;
    bool operator==(const Project &) const = default;
};
[[nodiscard]] Project makeDemoProject();
[[nodiscard]] float automationAt(const std::vector<AutomationPoint> &points, double beat) noexcept;
} // namespace forge
