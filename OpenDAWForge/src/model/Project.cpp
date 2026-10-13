#include "Project.h"
#include "core/Constants.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace forge
{
double TempoMap::bpmAt(double beat) const noexcept
{
    double bpm = 120;
    for (const auto &p : points)
    {
        if (p.beat > beat)
            break;
        bpm = p.bpm;
    }
    return bpm;
}
double TempoMap::secondsAt(double beat) const noexcept
{
    double seconds = 0, cursor = 0, bpm = 120;
    for (const auto &p : points)
    {
        if (p.beat > beat)
            break;
        seconds += (p.beat - cursor) * 60 / bpm;
        cursor = p.beat;
        bpm = p.bpm;
    }
    return seconds + (beat - cursor) * 60 / bpm;
}
Channel *Project::findChannel(ChannelID id) noexcept
{
    for (auto &c : channels)
        if (c.id == id)
            return &c;
    return nullptr;
}
const Channel *Project::findChannel(ChannelID id) const noexcept
{
    for (const auto &c : channels)
        if (c.id == id)
            return &c;
    return nullptr;
}
Pattern *Project::findPattern(PatternID id) noexcept
{
    for (auto &p : patterns)
        if (p.id == id)
            return &p;
    return nullptr;
}
const Pattern *Project::findPattern(PatternID id) const noexcept
{
    for (const auto &p : patterns)
        if (p.id == id)
            return &p;
    return nullptr;
}
Result Project::validate() const
{
    auto finite = [](double v) { return std::isfinite(v); };
    if (metadata.name.size() > 4096 || metadata.author.size() > 4096 || metadata.comments.size() > 4096)
        return Result::failure("Metadata text exceeds 4096 bytes");
    if (channels.size() > maxChannels || patterns.size() > 128 || tracks.size() > 128)
        return Result::failure("Project exceeds engine capacity");
    if (!finite(loopStart) || !finite(loopEnd) || loopStart < 0 || loopEnd <= loopStart ||
        loopEnd > 1000000 || !finite(masterVolume) || masterVolume < 0 || masterVolume > 1)
        return Result::failure("Invalid loop or master level");
    if (tempoMap.points.empty() || tempoMap.points.front().beat != 0)
        return Result::failure("Tempo map must start at beat zero");
    double previous = -1;
    for (const auto &t : tempoMap.points)
    {
        if (!finite(t.beat) || !finite(t.bpm) || t.beat <= previous || t.bpm < 20 || t.bpm > 400)
            return Result::failure("Invalid tempo map");
        previous = t.beat;
    }
    std::set<std::uint64_t> ids;
    auto unique = [&](auto id) { return id.value() != 0 && ids.insert(id.value()).second; };
    for (const auto &c : channels)
    {
        if (!unique(c.id) || c.name.size() > 4096 || static_cast<int>(c.instrument) > 4 ||
            c.samplePath.size() > 4096 || !finite(c.volume) || c.volume < 0 || c.volume > 1 ||
            !finite(c.pan) || std::abs(c.pan) > 1 || !finite(c.cutoff) || c.cutoff < 0 || c.cutoff > 1 ||
            !finite(c.delay) || c.delay < 0 || c.delay > 1 || !finite(c.reverb) || c.reverb < 0 ||
            c.reverb > 1 || !finite(c.compression) || c.compression < 0 || c.compression > 1 ||
            !finite(c.drive) || c.drive < 0 || c.drive > 1)
            return Result::failure("Invalid channel");
    }
    for (const auto &p : patterns)
    {
        if (p.name.size() > 4096 || !unique(p.id) || !finite(p.lengthBeats) || p.lengthBeats < 0.25 ||
            p.lengthBeats > 1024 || p.notes.size() > maxNotes)
            return Result::failure("Invalid pattern");
        for (const auto &n : p.notes)
            if (n.channel < 0 || static_cast<std::size_t>(n.channel) >= channels.size() || n.pitch < 0 ||
                n.pitch > 127 || !finite(n.startBeat) || n.startBeat < 0 || n.startBeat >= p.lengthBeats ||
                !finite(n.lengthBeats) || n.lengthBeats <= 0 || n.lengthBeats > 1024 || !finite(n.velocity) ||
                n.velocity < 0 || n.velocity > 1)
                return Result::failure("Invalid MIDI note");
    }
    for (const auto &t : tracks)
    {
        if (t.name.size() > 4096 || !unique(t.id) || t.clips.size() > 1024)
            return Result::failure("Invalid track");
        for (const auto &c : t.clips)
            if (!unique(c.id) || !findPattern(c.patternId) || !finite(c.startBeat) || c.startBeat < 0 ||
                c.startBeat > 1000000 || !finite(c.lengthBeats) || c.lengthBeats <= 0 ||
                c.lengthBeats > 4096 || std::abs(c.transpositionSemitones) > 127)
                return Result::failure("Invalid clip");
    }
    if (masterInserts.size() > 8)
        return Result::failure("Master chain exceeds eight inserts");
    std::size_t pluginBytes = 0;
    for (const auto &slot : masterInserts)
    {
        if (!unique(slot.id) || slot.name.size() > 4096 || slot.descriptionXml.empty() ||
            slot.descriptionXml.size() > 65536 || slot.state.size() > 4 * 1024 * 1024)
            return Result::failure("Invalid plugin slot");
        if (slot.normalizedParameters.size() > 10000)
            return Result::failure("Too many plugin parameters");
        for (auto value : slot.normalizedParameters)
            if (!finite(value) || value < 0 || value > 1)
                return Result::failure("Invalid plugin parameter");
        pluginBytes += slot.state.size();
    }
    if (pluginBytes > 8 * 1024 * 1024)
        return Result::failure("Plugin state exceeds eight MiB");
    if (automation.size() > 16)
        return Result::failure("Too many automation lanes");
    for (const auto &lane : automation)
    {
        if (lane.name.size() > 4096 || !unique(lane.id) || lane.channel < 0 ||
            static_cast<std::size_t>(lane.channel) >= channels.size() ||
            static_cast<unsigned>(lane.target) > 4 || static_cast<unsigned>(lane.interpolation) > 2 ||
            lane.points.empty() || lane.points.size() > 64)
            return Result::failure("Invalid automation lane");
        double last = -1;
        for (const auto &point : lane.points)
        {
            if (!finite(point.beat) || point.beat < 0 || point.beat > 1000000 || point.beat <= last ||
                !finite(point.value) || point.value < 0 || point.value > 1)
                return Result::failure("Invalid automation point");
            last = point.beat;
        }
    }
    return {};
}
std::uint64_t Project::nextId() const noexcept
{
    std::uint64_t id = 0;
    for (const auto &c : channels)
        id = std::max(id, c.id.value());
    for (const auto &p : patterns)
        id = std::max(id, p.id.value());
    for (const auto &t : tracks)
    {
        id = std::max(id, t.id.value());
        for (const auto &c : t.clips)
            id = std::max(id, c.id.value());
    }
    for (const auto &lane : automation)
        id = std::max(id, lane.id.value());
    for (const auto &slot : masterInserts)
        id = std::max(id, slot.id.value());
    return id + 1;
}
float automationAt(const std::vector<AutomationPoint> &points, double beat) noexcept
{
    if (points.empty())
        return 0;
    if (beat <= points.front().beat)
        return points.front().value;
    for (std::size_t i = 1; i < points.size(); ++i)
        if (beat < points[i].beat)
        {
            const auto &a = points[i - 1];
            const auto &b = points[i];
            const double d = b.beat - a.beat;
            return d > 0 ? a.value + static_cast<float>((beat - a.beat) / d) * (b.value - a.value) : b.value;
        }
    return points.back().value;
}
Project makeDemoProject()
{
    Project p;
    p.metadata.name = "Afterglow / Session 01";
    p.tempoMap.points = {{0, 115}};
    const char *names[] = {"Forge Kick",  "Paper Snare", "Closed Hat",
                           "Sub Current", "Glass Keys",  "Night Pad"};
    const std::uint32_t colors[] = {0xff9dbd77, 0xffcc7b78, 0xffd4b06f, 0xffb89ac9, 0xff79b9c4, 0xff879bbf};
    std::uint64_t id = 1;
    for (int i = 0; i < 6; ++i)
    {
        Channel c;
        c.id = ChannelID{id++};
        c.name = names[i];
        c.color = colors[i];
        c.instrument = i < 3 ? static_cast<InstrumentType>(i + 1) : InstrumentType::Synth;
        c.volume = i == 3 ? 0.45f : 0.6f;
        c.cutoff = i == 3 ? 0.12f : 0.65f;
        c.delay = i == 4 ? 0.22f : 0;
        p.channels.push_back(c);
    }
    for (int group = 0; group < 3; ++group)
    {
        Pattern pat;
        pat.id = PatternID{id++};
        pat.name = group == 0 ? "Drums / A" : group == 1 ? "Sub / A" : "Glass / A";
        pat.color = colors[group == 0 ? 0 : group == 1 ? 3 : 4];
        pat.lengthBeats = 8;
        if (group == 0)
        {
            for (int s = 0; s < 32; ++s)
            {
                if (s % 4 == 0)
                    pat.notes.push_back({0, 36, s * 0.25, 0.2, 0.9f});
                if (s % 8 == 4)
                    pat.notes.push_back({1, 38, s * 0.25, 0.2, 0.78f});
                if (s % 2 == 0)
                    pat.notes.push_back({2, 42, s * 0.25, 0.1, s % 4 == 0 ? 0.45f : 0.7f});
            }
        }
        else if (group == 1)
        {
            for (int s = 0; s < 8; ++s)
                pat.notes.push_back({3, s < 4 ? 36 : 43, static_cast<double>(s), 0.7, 0.7f});
        }
        else
        {
            const int pitches[] = {60, 64, 67, 71, 62, 65, 69, 72};
            for (int s = 0; s < 8; ++s)
            {
                pat.notes.push_back({4, pitches[s], static_cast<double>(s), 0.65, 0.6f});
                if (s % 2 == 0)
                    pat.notes.push_back({5, pitches[s] - 12, static_cast<double>(s), 1.8, 0.32f});
            }
        }
        p.patterns.push_back(pat);
        Track track;
        track.id = TrackID{id++};
        track.name = pat.name;
        track.color = pat.color;
        for (int b = 0; b < 4; ++b)
            track.clips.push_back({ClipID{id++}, pat.id, static_cast<double>(b * 8), 8, 0, false});
        p.tracks.push_back(track);
    }
    p.automation.push_back({TrackID{id++},
                            "Sub Current · Filter",
                            3,
                            AutomationTarget::Cutoff,
                            Interpolation::Smooth,
                            {{0, 0.12f}, {8, 0.65f}, {16, 0.18f}, {24, 0.48f}, {32, 0.12f}}});
    p.loopEnd = 32;
    return p;
}
} // namespace forge
