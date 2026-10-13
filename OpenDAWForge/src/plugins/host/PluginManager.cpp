#include "PluginManager.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace forge
{
PluginManager::PluginManager()
{
    juce::addDefaultFormatsToManager(formats_);
    buffer_.setSize(2, maximumBlock);
    midi_.ensureSize(32768);
}
PluginManager::~PluginManager()
{
    for (auto &plugin : plugins_)
        plugin->releaseResources();
}
Result PluginManager::scanInProcess(const juce::File &bundle, const juce::File &output)
{
    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager(formats);
    juce::XmlElement root("LSFactoryPlugins");
    for (auto *format : formats.getFormats())
    {
        if (!format->fileMightContainThisPluginType(bundle.getFullPathName()))
            continue;
        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile(found, bundle.getFullPathName());
        for (const auto *description : found)
            root.addChildElement(description->createXml().release());
    }
    if (root.getNumChildElements() == 0)
        return Result::failure("No supported plugin found");
    return root.writeTo(output) ? Result{} : Result::failure("Cannot write scan result");
}
Result PluginManager::scanIsolated(const juce::File &bundle, std::vector<PluginSlot> &slots)
{
    const auto output = juce::File::createTempFile(".xml");
    struct Cleanup
    {
        juce::File file;
        ~Cleanup()
        {
            file.deleteFile();
        }
    } cleanup{output};
    juce::ChildProcess process;
    juce::StringArray arguments{
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(), "--scan-plugin",
        bundle.getFullPathName(), output.getFullPathName()};
    if (!process.start(arguments))
        return Result::failure("Cannot start isolated plugin scanner");
    if (!process.waitForProcessToFinish(20000))
    {
        process.kill();
        return Result::failure("Plugin scan timed out");
    }
    if (process.getExitCode() != 0)
        return Result::failure("Plugin scanner failed or crashed: " +
                               process.readAllProcessOutput().substring(0, 2048).toStdString());
    if (output.getSize() > 1024 * 1024)
        return Result::failure("Plugin scan result exceeds limit");
    auto xml = juce::XmlDocument::parse(output);
    if (!xml || !xml->hasTagName("LSFactoryPlugins"))
        return Result::failure("Invalid scanner response");
    std::vector<PluginSlot> result;
    for (auto *entry : xml->getChildIterator())
    {
        juce::PluginDescription description;
        if (!description.loadFromXml(*entry))
            return Result::failure("Invalid plugin description");
        // VST3 descriptions may have unknown channel counts until instantiated.
        // replace() validates the actual bus layout before installing the effect.
        if (description.isInstrument)
            continue;
        result.push_back({PluginInstanceID{},
                          description.name.toStdString(),
                          entry->toString().toStdString(),
                          {},
                          false,
                          {}});
    }
    if (result.empty())
        return Result::failure("This milestone supports stereo effect plugins, not instrument plugins");
    slots = std::move(result);
    return {};
}
Result PluginManager::replace(const std::vector<PluginSlot> &slots, double rate, int block)
{
    if (!std::isfinite(rate) || rate <= 0 || block <= 0 || block > maximumBlock)
        return Result::failure("Invalid plugin processing configuration");
    if (slots.size() > 8)
        return Result::failure("Maximum eight master inserts");
    if (slots == slots_)
        return {};
    // Bypass changes preserve instances, editor parameters and tails.
    bool same = slots.size() == slots_.size();
    for (std::size_t i = 0; same && i < slots.size(); ++i)
        same = slots[i].id == slots_[i].id && slots[i].descriptionXml == slots_[i].descriptionXml &&
               slots[i].state == slots_[i].state &&
               slots[i].normalizedParameters == slots_[i].normalizedParameters;
    if (same)
    {
        slots_ = slots;
        return {};
    }
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> staging;
    for (const auto &slot : slots)
    {
        auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(slot.descriptionXml.c_str()));
        juce::PluginDescription description;
        if (!xml || !description.loadFromXml(*xml))
            return Result::failure("Invalid saved plugin description");
        if (description.isInstrument)
            return Result::failure("Instrument insert is unsupported");
        juce::String error;
        auto instance = formats_.createPluginInstance(description, rate, block, error);
        if (!instance)
            return Result::failure("Cannot load " + slot.name + ": " + error.toStdString());
        instance->disableNonMainBuses();
        auto layout = instance->getBusesLayout();
        if (layout.inputBuses.size() != 1 || layout.outputBuses.size() != 1)
            return Result::failure("Plugin requires unsupported buses");
        layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
        if (!instance->setBusesLayout(layout))
            return Result::failure("Plugin does not support stereo IO");
        instance->setPlayHead(this);
        instance->setRateAndBufferSizeDetails(rate, block);
        if (!slot.state.empty())
            instance->setStateInformation(slot.state.data(), static_cast<int>(slot.state.size()));
        instance->prepareToPlay(rate, block);
        if (!slot.normalizedParameters.empty())
        {
            const auto &parameters = instance->getParameters();
            if (static_cast<std::size_t>(parameters.size()) != slot.normalizedParameters.size())
                return Result::failure("Plugin parameter layout changed since saving");
            for (int i = 0; i < parameters.size(); ++i)
                parameters[i]->setValueNotifyingHost(slot.normalizedParameters[static_cast<std::size_t>(i)]);
        }
        staging.push_back(std::move(instance));
    }
    for (auto &plugin : plugins_)
        plugin->releaseResources();
    plugins_ = std::move(staging);
    slots_ = slots;
    rate_ = rate;
    return {};
}
void PluginManager::prepare(double rate, int block)
{
    rate_ = rate;
    for (auto &p : plugins_)
    {
        p->releaseResources();
        p->setRateAndBufferSizeDetails(rate, block);
        p->prepareToPlay(rate, block);
    }
}
void PluginManager::process(float *left, float *right, int samples, double beat, double bpm,
                            bool playing) noexcept
{
    if (plugins_.empty() || !left || !right)
        return;
    for (int offset = 0; offset < samples; offset += maximumBlock)
    {
        const int count = std::min(maximumBlock, samples - offset);
        buffer_.copyFrom(0, 0, left + offset, count);
        buffer_.copyFrom(1, 0, right + offset, count);
        juce::AudioBuffer<float> block(buffer_.getArrayOfWritePointers(), 2, count);
        position_.setBpm(bpm);
        position_.setPpqPosition(beat + offset * bpm / (60 * rate_));
        position_.setIsPlaying(playing);
        position_.setTimeInSamples(static_cast<juce::int64>(std::llround((beat * 60 / bpm) * rate_)) +
                                   offset);
        for (std::size_t i = 0; i < plugins_.size(); ++i)
            if (!slots_[i].bypassed)
            {
                midi_.clear();
                plugins_[i]->processBlock(block, midi_);
            }
        juce::FloatVectorOperations::copy(left + offset, block.getReadPointer(0), count);
        juce::FloatVectorOperations::copy(right + offset, block.getReadPointer(1), count);
    }
}
std::vector<PluginSlot> PluginManager::captureState()
{
    auto result = slots_;
    for (std::size_t i = 0; i < plugins_.size(); ++i)
    {
        juce::MemoryBlock state;
        plugins_[i]->getStateInformation(state);
        if (state.getSize() > 4 * 1024 * 1024)
            throw std::runtime_error("Plugin state exceeds four MiB");
        const auto *bytes = static_cast<const std::uint8_t *>(state.getData());
        if (state.getSize())
            result[i].state.assign(bytes, bytes + state.getSize());
        else
            result[i].state.clear();
    }
    for (std::size_t i = 0; i < plugins_.size(); ++i)
    {
        result[i].normalizedParameters.clear();
        const auto &parameters = plugins_[i]->getParameters();
        if (parameters.size() > 10000)
            throw std::runtime_error("Plugin parameter count exceeds limit");
        for (auto *parameter : parameters)
            result[i].normalizedParameters.push_back(parameter->getValue());
    }
    slots_ = result;
    return result;
}
juce::AudioPluginInstance *PluginManager::processor(std::size_t index) noexcept
{
    return index < plugins_.size() ? plugins_[index].get() : nullptr;
}
int PluginManager::latencySamples() const noexcept
{
    int latency = 0;
    for (std::size_t i = 0; i < plugins_.size(); ++i)
        if (!slots_[i].bypassed)
            latency += plugins_[i]->getLatencySamples();
    return latency;
}
} // namespace forge
