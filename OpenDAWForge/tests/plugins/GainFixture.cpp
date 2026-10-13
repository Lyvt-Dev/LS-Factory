#include <algorithm>
#include <cmath>
#include <cstring>
#include <juce_audio_processors/juce_audio_processors.h>
class GainFixture final : public juce::AudioProcessor
{
  public:
    GainFixture()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        gain_ = new juce::AudioParameterFloat(juce::ParameterID{"gain", 1}, "Gain", 0.0f, 1.0f, 0.5f);
        addParameter(gain_);
    }
    const juce::String getName() const override
    {
        return "Forge Gain Fixture";
    }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layout) const override
    {
        return layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
               layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float> &audio, juce::MidiBuffer &) override
    {
        audio.applyGain(gain_->get());
    }
    double getTailLengthSeconds() const override
    {
        return 0;
    }
    bool acceptsMidi() const override
    {
        return false;
    }
    bool producesMidi() const override
    {
        return false;
    }
    bool isMidiEffect() const override
    {
        return false;
    }
    bool hasEditor() const override
    {
        return true;
    }
    juce::AudioProcessorEditor *createEditor() override
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return "Default";
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &state) override
    {
        const float value = gain_->get();
        state.replaceAll(&value, sizeof value);
    }
    void setStateInformation(const void *data, int size) override
    {
        if (size == sizeof(float))
        {
            float value = 0;
            std::memcpy(&value, data, sizeof value);
            if (std::isfinite(value))
                gain_->setValueNotifyingHost(std::clamp(value, 0.0f, 1.0f));
        }
    }

  private:
    juce::AudioParameterFloat *gain_ = nullptr;
};
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new GainFixture();
}
