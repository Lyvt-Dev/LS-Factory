#pragma once
#include "RenderEngine.h"
#include "plugins/host/PluginManager.h"
#include <juce_audio_utils/juce_audio_utils.h>
namespace forge
{
class AudioEngine final : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
  public:
    AudioEngine();
    ~AudioEngine() override;
    juce::String initialise();
    void audioDeviceIOCallbackWithContext(const float *const *, int, float *const *, int, int,
                                          const juce::AudioIODeviceCallbackContext &) override;
    void audioDeviceAboutToStart(juce::AudioIODevice *) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage(juce::MidiInput *, const juce::MidiMessage &) override;
    RenderEngine renderer;
    juce::AudioDeviceManager devices;
    juce::String status = "Audio not initialised";
    std::atomic<float> cpuPercent{0};
    std::atomic<std::uint64_t> midiDrops{0};
    Result startRecording(const juce::File &, bool offline = false);
    void stopRecording();
    bool recordingAudio() const noexcept
    {
        return recording_.load();
    }
    void captureInput(const float *const *, int channels, int samples) noexcept;
    std::atomic<std::uint64_t> recordDrops{0};
    Result loadSample(std::size_t channel, const juce::File &, bool suspendDevice = true);
    Result restoreSamples(const Project &, bool suspendDevice = true);
    PluginManager plugins;
    Result restorePlugins(const std::vector<PluginSlot> &slots);
    Result capturePlugins(Project &project);
    bool pluginSmokeTest(const juce::File &fixture, const juce::File &directory);
    std::atomic<double> pluginTempo{120};
    std::atomic<float> outputPeakLeft{0}, outputPeakRight{0};
    Result exportWave(const Project &, const juce::File &);

  private:
    juce::TimeSliceThread writerThread_{"LS Factory recorder"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer_;
    std::atomic<bool> recording_{false};
    double deviceRate_ = 48000;
    std::unique_ptr<juce::MidiInput> input_;
};
} // namespace forge
