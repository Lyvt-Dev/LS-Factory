#include "Synth.h"
#include "core/Math.h"
#include <algorithm>
#include <cmath>
namespace forge
{
void Synth::prepare(double rate)
{
    rate_ = rate;
    for (auto &v : voices_)
        v.envelope.prepare(rate);
    for (auto &d : delay_)
        d.prepare(rate);
    for (auto &r : reverb_)
        r.prepare(rate);
    for (auto &c : compressor_)
        c.prepare(rate);
    allNotesOff();
}
void Synth::allNotesOff() noexcept
{
    for (auto &v : voices_)
        v.envelope.reset();
    for (auto &d : delay_)
        d.reset();
}
void Synth::noteOn(int channel, int pitch, float velocity, InstrumentType type) noexcept
{
    if (channel < 0 || static_cast<std::size_t>(channel) >= maxChannels)
        return;
    Voice *voice = nullptr;
    for (auto &v : voices_)
        if (!v.envelope.active())
        {
            voice = &v;
            break;
        }
    if (!voice)
        voice = &*std::min_element(voices_.begin(), voices_.end(),
                                   [](auto &a, auto &b) { return a.age < b.age; });
    auto &v = *voice;
    v.channel = channel;
    v.pitch = pitch;
    v.velocity = velocity;
    v.type = type;
    v.elapsed = 0;
    v.samplePosition = 0;
    v.filter = 0;
    v.age = ++age_;
    v.a.reset();
    v.b.reset();
    if (type == InstrumentType::Synth || type == InstrumentType::Sampler)
        v.envelope.set(0.008f, 0.2f, 0.45f, 0.2f);
    else
        v.envelope.set(0.001f, type == InstrumentType::Hat ? 0.05f : 0.22f, 0, 0.04f);
    v.envelope.on();
}
void Synth::noteOff(int channel, int pitch) noexcept
{
    for (auto &v : voices_)
        if (v.envelope.active() && v.channel == channel && v.pitch == pitch &&
            (v.type == InstrumentType::Synth || v.type == InstrumentType::Sampler))
            v.envelope.off();
}
std::size_t Synth::activeVoices() const noexcept
{
    return static_cast<std::size_t>(
        std::count_if(voices_.begin(), voices_.end(), [](auto &v) { return v.envelope.active(); }));
}
void Synth::renderSample(const PlaybackSnapshot &p, float &left, float &right,
                         std::array<float, maxChannels> *peaks) noexcept
{
    std::array<float, maxChannels> sums{};
    for (auto &v : voices_)
    {
        if (!v.envelope.active())
            continue;
        const auto &c = p.channels[static_cast<std::size_t>(v.channel)];
        float raw = 0;
        switch (v.type)
        {
        case InstrumentType::Synth:
        {
            const auto freq = midiFrequency(v.pitch);
            raw = (v.a.next(freq, rate_, Wave::Saw) + v.b.next(freq * 1.003, rate_, Wave::Sine)) * 0.35f;
            break;
        }
        case InstrumentType::Kick:
            raw = v.a.next(45 + 135 * std::exp(-v.elapsed * 35), rate_, Wave::Sine);
            break;
        case InstrumentType::Snare:
            raw = 0.75f * v.noise.next(1, rate_, Wave::Noise) + 0.25f * v.a.next(170, rate_, Wave::Triangle);
            break;
        case InstrumentType::Sampler:
        {
            const auto &sample = samples_[static_cast<std::size_t>(v.channel)];
            const double position = c.reversed
                                        ? static_cast<double>(sample.mono.size()) - 2 - v.samplePosition
                                        : v.samplePosition;
            raw = sample.read(position);
            v.samplePosition += std::exp2((v.pitch - sample.rootPitch) / 12.0) * sample.sampleRate / rate_;
            if (v.samplePosition >= static_cast<double>(sample.mono.size()))
                v.envelope.reset();
            break;
        }
        case InstrumentType::Hat:
            raw = v.noise.next(1, rate_, Wave::Noise);
            raw -= v.filter;
            break;
        }
        const double cutoff = 80 + 16000 * c.cutoff * c.cutoff;
        const float coefficient = static_cast<float>(1 - std::exp(-2 * pi * cutoff / rate_));
        v.filter += coefficient * (raw - v.filter);
        const float signal = v.type == InstrumentType::Hat ? raw * 0.32f : v.filter;
        sums[static_cast<std::size_t>(v.channel)] += signal * v.envelope.next() * v.velocity;
        v.elapsed += 1 / rate_;
    }
    left = right = 0;
    for (std::size_t i = 0; i < p.channelCount; ++i)
    {
        const auto &c = p.channels[i];
        float signal = delay_[i].process(sums[i], c.delay);
        signal = reverb_[i].process(signal, c.reverb);
        if (c.drive > 0)
        {
            const float drive = 1 + c.drive * 8;
            signal = std::tanh(signal * drive) / std::tanh(drive);
        }
        if (c.compression > 0)
            signal = compressor_[i].process(signal, signal, -36 * c.compression);
        if (!c.audible)
            continue;
        if (peaks)
            (*peaks)[i] = std::max((*peaks)[i], std::abs(signal * c.volume));
        const double angle = (c.pan + 1) * pi / 4;
        left += signal * c.volume * static_cast<float>(std::cos(angle));
        right += signal * c.volume * static_cast<float>(std::sin(angle));
    }
    left = std::tanh(left * p.master);
    right = std::tanh(right * p.master);
}
} // namespace forge
