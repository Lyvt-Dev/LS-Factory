#include "Scheduler.h"
#include "core/Time.h"
#include <algorithm>
#include <cmath>
namespace forge
{
Result compilePlayback(const Project &p, PatternID selected, bool song, PlaybackSnapshot &output)
{
    if (auto result = p.validate(); !result)
        return result;
    PlaybackSnapshot s;
    s.channelCount = p.channels.size();
    s.bpm = p.tempoMap.bpmAt(0);
    s.loop = p.loopEnabled;
    s.loopStart = p.loopStart;
    s.loopEnd = p.loopEnd;
    s.master = p.masterVolume;
    if (p.tempoMap.points.size() != 1)
        return Result::failure("Realtime tempo changes are not supported in this engine version");
    const bool solo =
        std::any_of(p.channels.begin(), p.channels.end(), [](const auto &c) { return c.soloed; });
    for (std::size_t i = 0; i < s.channelCount; ++i)
    {
        const auto &c = p.channels[i];
        s.channels[i] = {
            c.instrument,     c.volume, c.pan,         c.cutoff, c.delay, !c.muted && (!solo || c.soloed),
            c.sampleReversed, c.reverb, c.compression, c.drive};
    }
    auto add = [&](const Pattern &pattern, double start, double length, int transpose)
    {
        for (double repeat = 0; repeat < length; repeat += pattern.lengthBeats)
            for (const auto &n : pattern.notes)
            {
                if (repeat + n.startBeat >= length)
                    continue;
                if (s.noteCount == maxNotes)
                    return false;
                s.notes[s.noteCount++] = {
                    n.channel, std::clamp(n.pitch + transpose, 0, 127), start + repeat + n.startBeat,
                    start + std::min(length, repeat + n.startBeat + n.lengthBeats), n.velocity};
            }
        return true;
    };
    if (song)
    {
        for (const auto &t : p.tracks)
            for (const auto &c : t.clips)
                if (!c.muted &&
                    !add(*p.findPattern(c.patternId), c.startBeat, c.lengthBeats, c.transpositionSemitones))
                    return Result::failure("Arrangement exceeds 1024 scheduled notes");
    }
    else
    {
        const auto *pattern = p.findPattern(selected);
        if (!pattern)
            return Result::failure("No pattern selected");
        s.loopStart = 0;
        s.loopEnd = pattern->lengthBeats;
        s.loop = true;
        if (!add(*pattern, 0, pattern->lengthBeats, 0))
            return Result::failure("Pattern exceeds engine capacity");
    }
    if (song)
        for (const auto &lane : p.automation)
        {
            auto &a = s.automation[s.automationCount++];
            a.channel = lane.channel;
            a.target = lane.target;
            a.interpolation = lane.interpolation;
            a.count = lane.points.size();
            std::copy(lane.points.begin(), lane.points.end(), a.points.begin());
        }
    output = s;
    return {};
}
void applyAutomation(PlaybackSnapshot &p, double beat) noexcept
{
    for (std::size_t i = 0; i < p.automationCount; ++i)
    {
        const auto &a = p.automation[i];
        if (a.count == 0)
            continue;
        float value = a.points[0].value;
        for (std::size_t j = 1; j < a.count; ++j)
        {
            const auto &start = a.points[j - 1];
            const auto &end = a.points[j];
            if (beat >= end.beat)
            {
                value = end.value;
                continue;
            }
            if (beat > start.beat)
            {
                float t = static_cast<float>((beat - start.beat) / (end.beat - start.beat));
                if (a.interpolation == Interpolation::Step)
                    t = 0;
                else if (a.interpolation == Interpolation::Smooth)
                    t = t * t * (3 - 2 * t);
                value = start.value + (end.value - start.value) * t;
            }
            break;
        }
        auto &c = p.channels[static_cast<std::size_t>(a.channel)];
        switch (a.target)
        {
        case AutomationTarget::Volume:
            c.volume = value;
            break;
        case AutomationTarget::Pan:
            c.pan = value * 2 - 1;
            break;
        case AutomationTarget::Cutoff:
            c.cutoff = value;
            break;
        case AutomationTarget::Delay:
            c.delay = value;
            break;
        case AutomationTarget::Reverb:
            c.reverb = value;
            break;
        }
    }
}
EventBlock collectEvents(const PlaybackSnapshot &p, double first, int samples, double rate) noexcept
{
    EventBlock block;
    if (samples <= 0 || rate <= 0)
        return block;
    const double samplesPerBeat = beatsToSamples(1, p.bpm, rate);
    for (std::size_t i = 0; i < p.noteCount; ++i)
    {
        const auto &n = p.notes[i];
        for (int end = 0; end < 2; ++end)
        {
            const double at = end ? n.endBeat : n.startBeat;
            const auto offset = static_cast<long long>(std::llround((at - first) * samplesPerBeat));
            if (offset < 0 || offset >= samples)
                continue;
            if (block.count == maxEvents)
            {
                block.overflow = true;
                return block;
            }
            block.events[block.count++] = {static_cast<int>(offset), n.channel, n.pitch, n.velocity,
                                           end == 0};
        }
    }
    // Insertion sort is bounded and never allocates (std::stable_sort may allocate).
    for (std::size_t i = 1; i < block.count; ++i)
    {
        const auto e = block.events[i];
        std::size_t j = i;
        while (j > 0 && (block.events[j - 1].offset > e.offset ||
                         (block.events[j - 1].offset == e.offset && block.events[j - 1].on && !e.on)))
        {
            block.events[j] = block.events[j - 1];
            --j;
        }
        block.events[j] = e;
    }
    return block;
}
} // namespace forge
