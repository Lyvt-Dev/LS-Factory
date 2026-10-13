#pragma once
#include "core/Time.h"
#include <cstdint>
namespace forge
{
class Transport
{
  public:
    void prepare(double sampleRate) noexcept
    {
        rate_ = sampleRate;
    }
    void setTempo(double bpm) noexcept;
    void play() noexcept
    {
        playing_ = true;
    }
    void pause() noexcept
    {
        playing_ = false;
    }
    void stop() noexcept
    {
        playing_ = false;
        sample_ = 0;
    }
    void seek(std::int64_t sample) noexcept
    {
        sample_ = sample < 0 ? 0 : sample;
    }
    void advance(int samples) noexcept
    {
        if (playing_)
            sample_ += samples;
    }
    [[nodiscard]] bool playing() const noexcept
    {
        return playing_;
    }
    [[nodiscard]] double beat() const noexcept
    {
        return samplesToBeats(sample_, bpm_, rate_);
    }
    [[nodiscard]] std::int64_t sample() const noexcept
    {
        return sample_;
    }

  private:
    double rate_ = 48000, bpm_ = 120;
    std::int64_t sample_ = 0;
    bool playing_ = false;
};
} // namespace forge
