#pragma once
#include "core/Constants.h"
#include "model/Project.h"
#include <array>
#include <span>
namespace forge
{
struct ScheduledNote
{
    int channel = 0, pitch = 60;
    double startBeat = 0, endBeat = 1;
    float velocity = 0.8f;
};
struct PlaybackChannel
{
    InstrumentType instrument = InstrumentType::Synth;
    float volume = 0.6f, pan = 0, cutoff = 0.6f, delay = 0;
    bool audible = true;
    bool reversed = false;
    float reverb = 0, compression = 0, drive = 0;
};
struct PlaybackAutomation
{
    int channel = 0;
    AutomationTarget target = AutomationTarget::Volume;
    Interpolation interpolation = Interpolation::Linear;
    std::array<AutomationPoint, 64> points{};
    std::size_t count = 0;
};
struct PlaybackSnapshot
{
    std::array<PlaybackChannel, maxChannels> channels{};
    std::array<ScheduledNote, maxNotes> notes{};
    std::size_t channelCount = 0, noteCount = 0;
    std::array<PlaybackAutomation, 16> automation{};
    std::size_t automationCount = 0;
    double bpm = 120, loopStart = 0, loopEnd = 16;
    bool loop = true;
    float master = 0.8f;
};
struct Event
{
    int offset = 0, channel = 0, pitch = 60;
    float velocity = 0;
    bool on = false;
};
struct EventBlock
{
    std::array<Event, maxEvents> events{};
    std::size_t count = 0;
    bool overflow = false;
};
// UI-thread compilation; rejects projects that cannot fit the bounded audio snapshot.
void applyAutomation(PlaybackSnapshot &, double beat) noexcept;
Result compilePlayback(const Project &, PatternID selectedPattern, bool songMode, PlaybackSnapshot &output);
// Half-open sample interval. Release events precede starts at equal timestamps.
[[nodiscard]] EventBlock collectEvents(const PlaybackSnapshot &, double firstBeat, int samples,
                                       double rate) noexcept;
} // namespace forge
