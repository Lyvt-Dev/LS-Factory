#include "AudioEngine.h"
#include "persistence/BinaryFormat.h"
#include <chrono>
#include <iostream>
namespace forge
{
AudioEngine::AudioEngine()
{
    writerThread_.startThread();
}
AudioEngine::~AudioEngine()
{
    stopRecording();
    writerThread_.stopThread(3000);
    if (input_)
        input_->stop();
    devices.removeAudioCallback(this);
    devices.closeAudioDevice();
    delete pendingAudition_.exchange(nullptr);
    delete activeAudition_;
    Audition *retired = nullptr;
    while (retiredAuditions_.pop(retired))
        delete retired;
}
juce::String AudioEngine::initialise()
{
    const auto error = devices.initialise(0, 2, nullptr, true);
    if (error.isNotEmpty())
    {
        status = "No audio device: " + error;
        return error;
    }
    devices.addAudioCallback(this);
    status = "Audio ready";
    const auto inputs = juce::MidiInput::getAvailableDevices();
    if (!inputs.isEmpty())
    {
        input_ = juce::MidiInput::openDevice(inputs[0].identifier, this);
        if (input_)
            input_->start();
    }
    return {};
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice *device)
{
    deviceRate_ = device->getCurrentSampleRate();
    auditionRate_.store(deviceRate_);
    stopAudition();
    renderer.prepare(deviceRate_);
    plugins.prepare(deviceRate_, PluginManager::maximumBlock);
}
void AudioEngine::audioDeviceStopped()
{
    renderer.setPlaying(false);
}
void AudioEngine::handleIncomingMidiMessage(juce::MidiInput *, const juce::MidiMessage &message)
{
    if (message.isNoteOn() || message.isNoteOff())
        if (!renderer.midi({message.getNoteNumber(), message.getFloatVelocity(), message.isNoteOn(),
                            renderer.midiChannel()}))
            ++midiDrops;
}
void AudioEngine::audioDeviceIOCallbackWithContext(const float *const *input, int inputChannels,
                                                   float *const *output, int channels, int samples,
                                                   const juce::AudioIODeviceCallbackContext &)
{
    const auto start = std::chrono::steady_clock::now();
    captureInput(input, inputChannels, samples);
    for (int c = 0; c < channels; ++c)
        if (output[c])
            juce::FloatVectorOperations::clear(output[c], samples);
    const auto firstBeat = renderer.positionBeat();
    renderer.render(channels > 0 ? output[0] : nullptr, channels > 1 ? output[1] : nullptr, samples);
    if (channels >= 2 && output[0] && output[1])
    {
        plugins.process(output[0], output[1], samples, firstBeat, pluginTempo.load(), renderer.playing());
        mixAudition(output[0], output[1], samples);
        const auto leftRange = juce::FloatVectorOperations::findMinAndMax(output[0], samples),
                   rightRange = juce::FloatVectorOperations::findMinAndMax(output[1], samples);
        outputPeakLeft.store(std::max(std::abs(leftRange.getStart()), std::abs(leftRange.getEnd())));
        outputPeakRight.store(std::max(std::abs(rightRange.getStart()), std::abs(rightRange.getEnd())));
    }
    else
        mixAudition(channels > 0 ? output[0] : nullptr, nullptr, samples);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const double rate = deviceRate_;
    cpuPercent.store(static_cast<float>(elapsed * rate / std::max(1, samples) * 100));
}
Result AudioEngine::decodeAudio(const juce::File &file, SampleData &data)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples < 2 ||
        reader->lengthInSamples > reader->sampleRate * 60)
        return Result::failure("Unsupported or oversized audio sample");
    const int count = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(2, count);
    if (!reader->read(&buffer, 0, count, 0, true, true))
        return Result::failure("Sample decoding failed");
    data.sampleRate = reader->sampleRate;
    data.mono.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        data.mono[static_cast<size_t>(i)] =
            (buffer.getSample(0, i) + buffer.getSample(reader->numChannels > 1 ? 1 : 0, i)) * .5f;
    return {};
}
Result AudioEngine::audition(Channel channel, SampleData sample, double seconds)
{
    Audition *retired = nullptr;
    while (retiredAuditions_.pop(retired))
        delete retired;
    channel.id = ChannelID{1};
    Project project;
    project.channels.push_back(channel);
    if (auto r = project.validate(); !r)
        return r;
    auto preview = std::make_unique<Audition>();
    const double rate = auditionRate_.load();
    preview->engine.prepare(rate);
    PlaybackSnapshot state;
    if (auto r = compilePlayback(project, PatternID{}, true, state); !r)
        return r;
    preview->engine.publish(state);
    if (channel.instrument == InstrumentType::Sampler)
    {
        if (sample.mono.empty())
            return Result::failure("Sample preview is empty");
        preview->engine.setSample(0, std::move(sample));
    }
    preview->engine.midi({60, .8f, true, 0, false});
    preview->remaining = static_cast<std::int64_t>(juce::jlimit(.1, 10.0, seconds) * rate);
    stopAudition_.store(false);
    delete pendingAudition_.exchange(preview.release());
    auditionActive_.store(true);
    return {};
}
Result AudioEngine::auditionProject(const Project &project, PatternID pattern, double seconds)
{
    Audition *retired = nullptr;
    while (retiredAuditions_.pop(retired))
        delete retired;
    auto preview = std::make_unique<Audition>();
    const auto rate = auditionRate_.load();
    preview->engine.prepare(rate);
    PlaybackSnapshot state;
    if (auto r = compilePlayback(project, pattern, false, state); !r)
        return r;
    state.loop = false;
    preview->engine.publish(state);
    for (size_t i = 0; i < project.channels.size(); ++i)
        if (project.channels[i].instrument == InstrumentType::Sampler)
        {
            SampleData sample;
            if (auto r = decodeAudio(juce::File(project.channels[i].samplePath), sample); !r)
                return r;
            preview->engine.setSample(i, std::move(sample));
        }
    preview->engine.setPlaying(true);
    preview->remaining = static_cast<std::int64_t>(juce::jlimit(.1, 30.0, seconds) * rate);
    stopAudition_.store(false);
    delete pendingAudition_.exchange(preview.release());
    auditionActive_.store(true);
    return {};
}
void AudioEngine::mixAudition(float *left, float *right, int samples) noexcept
{
    if (auto *next = pendingAudition_.exchange(nullptr))
    {
        // One request producer drains retirement before every publish: at most two retired
        // pointers can await it. The fixed ring therefore never fills under this contract.
        if (activeAudition_)
        {
            const bool queued = retiredAuditions_.push(activeAudition_);
            jassert(queued);
            (void)queued;
        }
        activeAudition_ = next;
    }
    if (!activeAudition_)
        return;
    if (stopAudition_.load())
        activeAudition_->remaining = 0;
    for (int offset = 0; offset < samples && activeAudition_->remaining > 0;)
    {
        const int count = std::min(512, samples - offset);
        activeAudition_->engine.render(auditionLeft_.data(), auditionRight_.data(), count);
        for (int i = 0; i < count; ++i)
        {
            const float fade =
                static_cast<float>(std::clamp<std::int64_t>(activeAudition_->remaining, 0, 256)) / 256;
            if (left)
                left[offset + i] =
                    std::clamp(left[offset + i] + auditionLeft_[static_cast<size_t>(i)] * fade, -1.0f, 1.0f);
            if (right)
                right[offset + i] = std::clamp(
                    right[offset + i] + auditionRight_[static_cast<size_t>(i)] * fade, -1.0f, 1.0f);
            if (activeAudition_->remaining > 0)
                --activeAudition_->remaining;
        }
        offset += count;
    }
    if (activeAudition_->remaining == 0)
        auditionActive_.store(false);
}
Result AudioEngine::startRecording(const juce::File &file, bool offline)
{
    if (recording_.load())
        return Result::failure("Recording already active");
    if (!offline)
    {
        auto *device = devices.getCurrentAudioDevice();
        if (!device || device->getActiveInputChannels().isZero())
            return Result::failure("Enable an audio input in Audio settings before recording");
    }
    auto fileStream = file.createOutputStream();
    if (!fileStream)
        return Result::failure("Cannot create recording");
    fileStream->setPosition(0);
    fileStream->truncate();
    std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                     .withSampleRate(deviceRate_)
                                                     .withNumChannels(2)
                                                     .withBitsPerSample(24));
    if (!writer)
        return Result::failure("Cannot create recording writer");
    const bool play = renderer.playing();
    devices.removeAudioCallback(this);
    writer_ =
        std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), writerThread_, 32768);
    recordDrops.store(0);
    recording_.store(true);
    if (!offline)
    {
        devices.addAudioCallback(this);
        renderer.setPlaying(play);
    }
    return {};
}
void AudioEngine::stopRecording()
{
    if (!recording_.load())
        return;
    const bool play = renderer.playing();
    devices.removeAudioCallback(this);
    recording_.store(false);
    writer_.reset();
    if (devices.getCurrentAudioDevice())
    {
        devices.addAudioCallback(this);
        renderer.setPlaying(play);
    }
}
void AudioEngine::captureInput(const float *const *inputs, int channels, int samples) noexcept
{
    if (!writer_ || !recording_.load() || channels < 1 || !inputs || !inputs[0])
        return;
    const float *data[] = {inputs[0], channels > 1 && inputs[1] ? inputs[1] : inputs[0]};
    if (!writer_->write(data, samples))
        ++recordDrops;
}
Result AudioEngine::loadSample(std::size_t channel, const juce::File &file, bool suspend)
{
    if (channel >= maxChannels)
        return Result::failure("Invalid sampler channel");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader)
        return Result::failure("Unsupported or unreadable sample: " + file.getFileName().toStdString());
    if (reader->lengthInSamples < 2 ||
        reader->lengthInSamples > static_cast<juce::int64>(reader->sampleRate * 60))
        return Result::failure("Samples must be between 2 frames and 60 seconds");
    const int frames = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(2, frames);
    if (!reader->read(&buffer, 0, frames, 0, true, true))
        return Result::failure("Sample decoding failed");
    SampleData data;
    data.sampleRate = reader->sampleRate;
    data.mono.resize(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i)
        data.mono[static_cast<std::size_t>(i)] =
            (buffer.getSample(0, i) + buffer.getSample(reader->numChannels > 1 ? 1 : 0, i)) * 0.5f;
    const bool playing = renderer.playing();
    if (suspend)
        devices.removeAudioCallback(this);
    renderer.setSample(channel, std::move(data));
    if (suspend)
    {
        devices.addAudioCallback(this);
        renderer.setPlaying(playing);
    }
    return {};
}
Result AudioEngine::restoreSamples(const Project &project, bool suspend)
{
    const bool playing = renderer.playing();
    if (suspend)
        devices.removeAudioCallback(this);
    renderer.clearSamples();
    Result result;
    for (std::size_t i = 0; i < project.channels.size(); ++i)
    {
        const auto &c = project.channels[i];
        if (c.instrument == InstrumentType::Sampler)
        {
            result = loadSample(i, juce::File(c.samplePath), false);
            if (!result)
                break;
        }
    }
    if (suspend)
    {
        devices.addAudioCallback(this);
        renderer.setPlaying(playing);
    }
    return result;
}
Result AudioEngine::restorePlugins(const std::vector<PluginSlot> &slots)
{
    const bool playing = renderer.playing();
    devices.removeAudioCallback(this);
    Result result;
    try
    {
        result = plugins.replace(slots, deviceRate_, PluginManager::maximumBlock);
    }
    catch (const std::exception &e)
    {
        result = Result::failure(e.what());
    }
    if (devices.getCurrentAudioDevice())
    {
        devices.addAudioCallback(this);
        renderer.setPlaying(playing);
    }
    return result;
}
Result AudioEngine::capturePlugins(Project &project)
{
    if (project.masterInserts.empty())
        return {};
    const bool playing = renderer.playing();
    devices.removeAudioCallback(this);
    Result result;
    try
    {
        auto captured = plugins.captureState();
        bool matches = captured.size() == project.masterInserts.size();
        for (std::size_t i = 0; matches && i < captured.size(); ++i)
            matches = captured[i].id == project.masterInserts[i].id;
        if (matches)
            project.masterInserts = std::move(captured);
        else
            result = Result::failure("Plugin chain is not loaded; state capture unavailable");
    }
    catch (const std::exception &e)
    {
        result = Result::failure(e.what());
    }
    if (devices.getCurrentAudioDevice())
    {
        devices.addAudioCallback(this);
        renderer.setPlaying(playing);
    }
    return result;
}
Result AudioEngine::exportWave(const Project &project, const juce::File &file)
{
    PlaybackSnapshot snapshot;
    if (auto r = compilePlayback(project, PatternID{}, true, snapshot); !r)
        return r;
    snapshot.loop = false;
    auto render = std::make_unique<RenderEngine>();
    render->prepare(48000);
    render->publish(snapshot);
    render->setPlaying(true);
    for (std::size_t i = 0; i < project.channels.size(); ++i)
        if (project.channels[i].instrument == InstrumentType::Sampler)
        {
            const auto &c = project.channels[i];
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(
                formats.createReaderFor(juce::File(c.samplePath)));
            if (!reader || reader->lengthInSamples < 2 || reader->lengthInSamples > reader->sampleRate * 60)
                return Result::failure("Missing or invalid sample: " + c.samplePath);
            const int count = static_cast<int>(reader->lengthInSamples);
            juce::AudioBuffer<float> b(2, count);
            if (!reader->read(&b, 0, count, 0, true, true))
                return Result::failure("Cannot decode sample");
            SampleData data;
            data.sampleRate = reader->sampleRate;
            data.mono.resize(static_cast<std::size_t>(count));
            for (int j = 0; j < count; ++j)
                data.mono[static_cast<std::size_t>(j)] =
                    (b.getSample(0, j) + b.getSample(reader->numChannels > 1 ? 1 : 0, j)) * 0.5f;
            render->setSample(i, std::move(data));
        }
    double end = project.loopEnd;
    for (const auto &t : project.tracks)
        for (const auto &c : t.clips)
            end = std::max(end, c.startBeat + c.lengthBeats);
    auto total = static_cast<std::int64_t>((end * 60 / snapshot.bpm + 2) * 48000);
    auto fileStream = file.createOutputStream();
    if (!fileStream)
        return Result::failure("Cannot open WAV output");
    fileStream->setPosition(0);
    fileStream->truncate();
    std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(
        stream,
        juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
    if (!writer)
        return Result::failure("Cannot create WAV writer");
    PluginManager exportPlugins;
    if (auto r = exportPlugins.replace(project.masterInserts, 48000, PluginManager::maximumBlock); !r)
        return r;
    juce::AudioBuffer<float> buffer(2, 512);
    for (std::int64_t offset = 0; offset < total; offset += 512)
    {
        const int count = static_cast<int>(std::min<std::int64_t>(512, total - offset));
        const double firstBeat = render->positionBeat();
        render->render(buffer.getWritePointer(0), buffer.getWritePointer(1), count);
        exportPlugins.process(buffer.getWritePointer(0), buffer.getWritePointer(1), count, firstBeat,
                              snapshot.bpm, true);
        if (!writer->writeFromAudioSampleBuffer(buffer, 0, count))
            return Result::failure("WAV write failed");
    }
    return {};
}
bool AudioEngine::pluginSmokeTest(const juce::File &fixture, const juce::File &directory)
{
    std::vector<PluginSlot> scanned;
    auto scanResult = PluginManager::scanIsolated(fixture, scanned);
    if (!scanResult)
        std::cerr << scanResult.error << std::endl;
    if (!scanResult || scanned.empty())
        return (std::cerr << "Plugin smoke failed during isolated scan" << std::endl, false);
    auto slot = scanned.front();
    slot.id = PluginInstanceID{1};
    PluginManager chain;
    if (!chain.replace({slot}, 48000, 4096))
        return (std::cerr << "Plugin smoke failed during initial load" << std::endl, false);
    std::array<float, 512> left{}, right{};
    left.fill(0.8f);
    right.fill(-0.4f);
    chain.process(left.data(), right.data(), 512, 0, 120, true);
    if (std::abs(left[100] - 0.4f) > 1e-5f || std::abs(right[100] + 0.2f) > 1e-5f)
        return (std::cerr << "Plugin smoke failed during initial gain" << std::endl, false);
    auto *processor = chain.processor(0);
    if (!processor || processor->getParameters().isEmpty())
        return (std::cerr << "Plugin smoke failed during parameters" << std::endl, false);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditorAndMakeActive());
        if (!editor)
            return (std::cerr << "Plugin editor unavailable" << std::endl, false);
        // Native plugin child windows cannot be captured by JUCE's offscreen renderer.
        // Capture the host's fallback parameter editor instead; retain the native-editor creation check.
        juce::GenericAudioProcessorEditor parameterEditor(*processor);
        parameterEditor.setVisible(true);
        const auto image = parameterEditor.createComponentSnapshot(parameterEditor.getLocalBounds());
        juce::FileOutputStream stream(directory.getChildFile("plugin-editor.png"));
        stream.setPosition(0);
        stream.truncate();
        if (!juce::PNGImageFormat().writeImageToStream(image, stream))
            return false;
    }
    processor->getParameters()[0]->setValueNotifyingHost(0.25f);
    auto saved = chain.captureState();
    if (saved.size() != 1 || saved[0].state.empty())
        return (std::cerr << "Plugin smoke failed during state capture" << std::endl, false);
    auto project = makeDemoProject();
    saved[0].id = PluginInstanceID{project.nextId()};
    project.masterInserts = saved;
    const auto file = directory.getChildFile("plugin-state.odaw");
    if (!saveProject(project, file.getFullPathName().toStdString()))
        return (std::cerr << "Plugin smoke failed during project save" << std::endl, false);
    Project loaded;
    if (!loadProject(file.getFullPathName().toStdString(), loaded) || loaded != project)
        return (std::cerr << "Plugin smoke failed during project restore" << std::endl, false);
    PluginManager restored;
    if (!restored.replace(loaded.masterInserts, 48000, 4096))
        return (std::cerr << "Plugin smoke failed during plugin restore" << std::endl, false);
    left.fill(0.8f);
    right.fill(-0.4f);
    restored.process(left.data(), right.data(), 512, 0, 120, true);
    if (std::abs(left[100] - 0.2f) > 1e-5f || std::abs(right[100] + 0.1f) > 1e-5f)
        return (std::cerr << "Plugin smoke failed during restored gain" << std::endl, false);
    loaded.masterInserts[0].bypassed = true;
    if (!restored.replace(loaded.masterInserts, 48000, 4096))
        return (std::cerr << "Plugin smoke failed during bypass installation" << std::endl, false);
    left.fill(0.8f);
    right.fill(-0.4f);
    restored.process(left.data(), right.data(), 512, 0, 120, true);
    if (std::abs(left[100] - 0.8f) > 1e-5f)
        return (std::cerr << "Plugin smoke failed during bypass audio" << std::endl, false);
    auto twice = loaded.masterInserts;
    twice[0].bypassed = false;
    auto second = twice[0];
    second.id = PluginInstanceID{project.nextId() + 1};
    twice.push_back(second);
    if (!restored.replace(twice, 48000, 4096))
        return (std::cerr << "Plugin smoke failed during two insert installation" << std::endl, false);
    left.fill(0.8f);
    right.fill(-0.4f);
    restored.process(left.data(), right.data(), 512, 0, 120, true);
    if (std::abs(left[100] - 0.05f) > 1e-5f)
        return (std::cerr << "Plugin smoke failed during two insert gain" << std::endl, false);
    const auto oldSlots = restored.slots();
    auto invalid = twice;
    invalid[0].descriptionXml = "invalid";
    if (restored.replace(invalid, 48000, 4096) || restored.slots() != oldSlots)
        return (std::cerr << "Plugin smoke failed during failed replacement" << std::endl, false);
    return static_cast<bool>(exportWave(project, directory.getChildFile("plugin-demo.wav")));
}
} // namespace forge
