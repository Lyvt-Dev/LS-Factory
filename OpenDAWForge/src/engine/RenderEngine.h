#pragma once
#include "core/SpscQueue.h"
#include "instruments/Synth.h"
#include "midi/MidiRecorder.h"
#include <atomic>
#include <cstdint>
namespace forge
{
struct LiveMidi
{
    int pitch = 60;
    float velocity = 0;
    bool on = false;
    int channel = 0;
    bool recordable = true;
};
class RenderEngine
{
  public:
    void prepare(double rate);
    void setSample(std::size_t channel, SampleData sample)
    {
        synth_.setSample(channel, std::move(sample));
    }
    void clearSamples()
    {
        synth_.clearSamples();
    }
    bool publish(const PlaybackSnapshot &state) noexcept
    {
        return states_.push(state);
    }
    void setPlaying(bool play) noexcept
    {
        requestedPlaying_.store(play);
    }
    bool playing() const noexcept
    {
        return requestedPlaying_.load();
    }
    void seekBeat(double beat) noexcept
    {
        if (beat >= 0 && beat <= 1000000)
            seek_.store(beat);
    }
    void rewind() noexcept
    {
        rewind_.store(true);
    }
    bool midi(const LiveMidi &event) noexcept
    {
        return midi_.push(event);
    }
    void setMidiChannel(int channel) noexcept
    {
        midiChannel_.store(channel);
    }
    int midiChannel() const noexcept
    {
        return midiChannel_.load();
    }
    void setRecording(bool active) noexcept
    {
        recording_.store(active);
    }
    bool recording() const noexcept
    {
        return recording_.load();
    }
    bool capture(CapturedMidi &midi) noexcept
    {
        return captures_.pop(midi);
    }
    void render(float *left, float *right, int samples) noexcept;
    double positionBeat() const noexcept
    {
        return position_.load();
    }
    float peakLeft() const noexcept
    {
        return peakL_.load();
    }
    float peakRight() const noexcept
    {
        return peakR_.load();
    }
    float channelPeak(std::size_t index) const noexcept
    {
        return index < maxChannels ? channelPeaks_[index].load() : 0;
    }
    std::uint64_t overflows() const noexcept
    {
        return overflows_.load();
    }

  private:
    SpscQueue<CapturedMidi, 1024> captures_;
    std::atomic<bool> recording_{false};
    std::atomic<int> midiChannel_{0};
    SpscQueue<PlaybackSnapshot, 8> states_;
    SpscQueue<LiveMidi, 256> midi_;
    PlaybackSnapshot state_{};
    Synth synth_;
    std::atomic<double> seek_{-1};
    double rate_ = 48000;
    std::int64_t sample_ = 0;
    bool wasPlaying_ = false;
    std::atomic<bool> requestedPlaying_{false}, rewind_{false};
    std::atomic<double> position_{0};
    std::atomic<float> peakL_{0}, peakR_{0};
    std::array<std::atomic<float>, maxChannels> channelPeaks_{};
    std::atomic<std::uint64_t> overflows_{0};
};
} // namespace forge
