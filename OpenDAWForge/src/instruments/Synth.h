#pragma once
#include "Sampler.h"
#include "core/Constants.h"
#include "dsp/Compressor.h"
#include "dsp/Delay.h"
#include "dsp/Envelope.h"
#include "dsp/Oscillator.h"
#include "dsp/Reverb.h"
#include "engine/Scheduler.h"
#include <array>
namespace forge
{
class Synth
{
  public:
    void prepare(double rate);
    // Caller must suspend rendering before replacing sample storage.
    void setSample(std::size_t channel, SampleData sample)
    {
        samples_[channel] = std::move(sample);
    }
    void clearSamples()
    {
        for (auto &s : samples_)
            s = {};
    }
    void noteOn(int channel, int pitch, float velocity, InstrumentType type, bool preview = false) noexcept;
    void noteOff(int channel, int pitch, bool preview = false) noexcept;
    void allNotesOff() noexcept;
    void renderSample(const PlaybackSnapshot &, float &left, float &right,
                      std::array<float, maxChannels> *peaks = nullptr) noexcept;
    [[nodiscard]] std::size_t activeVoices() const noexcept;

  private:
    struct Voice
    {
        Envelope envelope;
        Oscillator a, b, noise;
        int channel = 0, pitch = 60;
        bool preview = false;
        InstrumentType type = InstrumentType::Synth;
        float velocity = 0, filter = 0;
        std::uint64_t age = 0;
        double elapsed = 0, samplePosition = 0;
    };
    std::array<Voice, maxVoices> voices_{};
    std::array<Delay, maxChannels> delay_{};
    std::array<SampleData, maxChannels> samples_{};
    std::array<Reverb, maxChannels> reverb_{};
    std::array<Compressor, maxChannels> compressor_{};
    double rate_ = 48000;
    std::uint64_t age_ = 0;
};
} // namespace forge
