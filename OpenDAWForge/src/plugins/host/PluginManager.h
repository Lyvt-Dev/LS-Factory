#pragma once
#include "model/Project.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace forge
{
// Control methods require suspended audio. The audio callback is the only process() caller.
class PluginManager final : private juce::AudioPlayHead
{
  public:
    static constexpr int maximumBlock = 4096;
    PluginManager();
    ~PluginManager();
    static Result scanInProcess(const juce::File &bundle, const juce::File &output);
    static Result scanIsolated(const juce::File &bundle, std::vector<PluginSlot> &descriptions);
    Result replace(const std::vector<PluginSlot> &slots, double rate, int block);
    void prepare(double rate, int block);
    void process(float *left, float *right, int samples, double firstBeat, double bpm, bool playing) noexcept;
    std::vector<PluginSlot> captureState();
    const std::vector<PluginSlot> &slots() const noexcept
    {
        return slots_;
    }
    juce::AudioPluginInstance *processor(std::size_t index) noexcept;
    int latencySamples() const noexcept;

  private:
    juce::AudioPluginFormatManager formats_;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins_;
    std::vector<PluginSlot> slots_;
    juce::AudioBuffer<float> buffer_;
    juce::MidiBuffer midi_;
    PositionInfo position_;
    double rate_ = 48000;
    juce::Optional<PositionInfo> getPosition() const override
    {
        return position_;
    }
};
} // namespace forge
