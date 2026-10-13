#include "RenderEngine.h"
#include "core/Time.h"
#include <algorithm>
#include <cmath>
namespace forge
{
void RenderEngine::prepare(double rate)
{
    rate_ = rate;
    synth_.prepare(rate);
    sample_ = 0;
    wasPlaying_ = false;
}
void RenderEngine::render(float *left, float *right, int samples) noexcept
{
    PlaybackSnapshot pending;
    bool changed = false;
    while (states_.pop(pending))
    {
        state_ = pending;
        changed = true;
    }
    if (changed)
        synth_.allNotesOff();
    const bool play = requestedPlaying_.load();
    if (wasPlaying_ && !play)
        synth_.allNotesOff();
    if (rewind_.exchange(false))
    {
        sample_ = 0;
        synth_.allNotesOff();
    }
    if (play && (!wasPlaying_ || changed))
    {
        const double beat = samplesToBeats(sample_, state_.bpm, rate_);
        for (std::size_t i = 0; i < state_.noteCount; ++i)
        {
            const auto &n = state_.notes[i];
            if (n.startBeat < beat && n.endBeat > beat)
                synth_.noteOn(n.channel, n.pitch, n.velocity,
                              state_.channels[static_cast<std::size_t>(n.channel)].instrument);
        }
    }
    wasPlaying_ = play;
    LiveMidi midi;
    while (midi_.pop(midi))
    {
        if (midi.on)
            synth_.noteOn(midi.channel, midi.pitch, midi.velocity,
                          state_.channels[static_cast<std::size_t>(midi.channel)].instrument);
        else
            synth_.noteOff(midi.channel, midi.pitch);
        if (recording_.load() && !captures_.push({midi.channel, midi.pitch, midi.velocity, midi.on,
                                                  samplesToBeats(sample_, state_.bpm, rate_)}))
            ++overflows_;
    }
    float peakL = 0, peakR = 0;
    std::array<float, maxChannels> peaks{};
    int cursor = 0;
    const auto loopStart =
        static_cast<std::int64_t>(std::llround(beatsToSamples(state_.loopStart, state_.bpm, rate_)));
    const auto loopEnd =
        std::max(loopStart + 1,
                 static_cast<std::int64_t>(std::llround(beatsToSamples(state_.loopEnd, state_.bpm, rate_))));
    while (cursor < samples)
    {
        if (play && state_.loop && sample_ >= loopEnd)
        {
            sample_ = loopStart;
            synth_.allNotesOff();
        }
        const int remaining = samples - cursor;
        const int length = play && state_.loop
                               ? static_cast<int>(std::min<std::int64_t>(remaining, loopEnd - sample_))
                               : remaining;
        if (length <= 0)
        {
            ++overflows_;
            break;
        }
        const auto events =
            play ? collectEvents(state_, samplesToBeats(sample_, state_.bpm, rate_), length, rate_)
                 : EventBlock{};
        if (events.overflow)
            ++overflows_;
        std::size_t event = 0;
        for (int i = 0; i < length; ++i)
        {
            while (event < events.count && events.events[event].offset == i)
            {
                const auto &e = events.events[event++];
                if (e.on)
                    synth_.noteOn(e.channel, e.pitch, e.velocity,
                                  state_.channels[static_cast<std::size_t>(e.channel)].instrument);
                else
                    synth_.noteOff(e.channel, e.pitch);
            }
            if (play)
                applyAutomation(state_, samplesToBeats(sample_ + i, state_.bpm, rate_));
            float l = 0, r = 0;
            synth_.renderSample(state_, l, r, &peaks);
            if (left)
                left[cursor + i] = l;
            if (right)
                right[cursor + i] = r;
            peakL = std::max(peakL, std::abs(l));
            peakR = std::max(peakR, std::abs(r));
        }
        if (play)
            sample_ += length;
        cursor += length;
    }
    position_.store(samplesToBeats(sample_, state_.bpm, rate_));
    peakL_.store(peakL);
    peakR_.store(peakR);
    for (std::size_t i = 0; i < maxChannels; ++i)
        channelPeaks_[i].store(peaks[i]);
}
} // namespace forge
