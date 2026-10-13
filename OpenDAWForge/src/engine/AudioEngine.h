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
    // Single producer: the workspace worker. Prepared previews swap at block boundaries;
    // retired renderers are deleted on the worker, never in the audio callback.
    Result audition(Channel, SampleData = {}, double seconds = 3.0);
    Result auditionProject(const Project &, PatternID, double seconds);
    void stopAudition() noexcept
    {
        stopAudition_.store(true);
        auditionActive_.store(false);
    }
    bool auditionActive() const noexcept
    {
        return auditionActive_.load();
    }
    static Result decodeAudio(const juce::File &, SampleData &);
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
    struct Audition
    {
        RenderEngine engine;
        std::int64_t remaining = 0;
    };
    std::atomic<Audition *> pendingAudition_{nullptr};
    Audition *activeAudition_ = nullptr;
    SpscQueue<Audition *, 32> retiredAuditions_;
    std::atomic<bool> stopAudition_{false}, auditionActive_{false};
    std::atomic<double> auditionRate_{48000};
    std::array<float, 512> auditionLeft_{}, auditionRight_{};
    void mixAudition(float *, float *, int) noexcept;
    juce::TimeSliceThread writerThread_{"LS Factory recorder"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer_;
    std::atomic<bool> recording_{false};
    double deviceRate_ = 48000;
    std::unique_ptr<juce::MidiInput> input_;
};
} // namespace forge
