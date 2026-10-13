#include "commands/UndoStack.h"
#include "core/SpscQueue.h"
#include "dsp/Compressor.h"
#include "dsp/Delay.h"
#include "dsp/EQ.h"
#include "dsp/Envelope.h"
#include "dsp/Reverb.h"
#include "engine/RenderEngine.h"
#include "engine/Scheduler.h"
#include "engine/Transport.h"
#include "model/Project.h"
#include "persistence/BinaryFormat.h"
#include "persistence/JsonExporter.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <fstream>
#include <memory>
#include <thread>
using namespace forge;
TEST_CASE("Beat conversion and transport", "[transport]")
{
    REQUIRE(beatsToSamples(1, 120, 48000) == 24000);
    REQUIRE(samplesToBeats(24000, 120, 48000) == 1);
    Transport t;
    t.prepare(48000);
    t.play();
    t.advance(24000);
    REQUIRE(t.beat() == 1);
    t.pause();
    t.advance(512);
    REQUIRE(t.sample() == 24000);
    t.stop();
    REQUIRE(t.sample() == 0);
}
TEST_CASE("Tempo map integrates segments", "[tempo]")
{
    TempoMap t;
    t.points = {{0, 120}, {4, 60}};
    REQUIRE(t.secondsAt(6) == 4);
    REQUIRE(t.bpmAt(5) == 60);
}
TEST_CASE("Project validation rejects invalid references and numbers", "[model]")
{
    auto p = makeDemoProject();
    REQUIRE(p.validate());
    p.patterns[0].notes[0].channel = 99;
    REQUIRE_FALSE(p.validate());
    p = makeDemoProject();
    p.channels[0].volume = NAN;
    REQUIRE_FALSE(p.validate());
}
TEST_CASE("Undo is transactional and redo is invalidated", "[commands]")
{
    auto p = makeDemoProject();
    const auto before = p;
    UndoStack u;
    REQUIRE(u.execute(p, "tempo", [](auto &x) { x.tempoMap.points[0].bpm = 128; }));
    REQUIRE(p != before);
    REQUIRE(u.undo(p));
    REQUIRE(p == before);
    REQUIRE(u.redo(p));
    REQUIRE(p.tempoMap.bpmAt(0) == 128);
    REQUIRE(u.undo(p));
    REQUIRE(u.execute(p, "pan", [](auto &x) { x.channels[0].pan = 0.25f; }));
    REQUIRE_FALSE(u.canRedo());
    const auto good = p;
    REQUIRE_FALSE(u.execute(p, "bad", [](auto &x) { x.channels[0].pan = 5; }));
    REQUIRE(p == good);
}
TEST_CASE("Scheduler fires inside blocks with half open boundaries", "[scheduler]")
{
    PlaybackSnapshot p;
    p.bpm = 120;
    p.noteCount = 2;
    p.notes[0] = {0, 60, 37.0 / 24000, 480.0 / 24000, 0.8f};
    p.notes[1] = {0, 62, 512.0 / 24000, 600.0 / 24000, 0.8f};
    auto e = collectEvents(p, 0, 512, 48000);
    REQUIRE(e.count == 2);
    REQUIRE(e.events[0].offset == 37);
    REQUIRE(e.events[0].on);
    REQUIRE(e.events[1].offset == 480);
    REQUIRE_FALSE(e.events[1].on);
    e = collectEvents(p, 512.0 / 24000, 512, 48000);
    REQUIRE(e.count == 2);
    REQUIRE(e.events[0].offset == 0);
}
TEST_CASE("Note off precedes note on at equal timestamps", "[scheduler]")
{
    PlaybackSnapshot p;
    p.noteCount = 2;
    p.notes[0] = {0, 60, 1, 2, 1};
    p.notes[1] = {0, 60, 0, 1, 1};
    auto e = collectEvents(p, 1, 64, 48000);
    REQUIRE(e.count == 2);
    REQUIRE_FALSE(e.events[0].on);
    REQUIRE(e.events[1].on);
}
TEST_CASE("Snapshots compile patterns and reject oversized songs", "[scheduler]")
{
    auto p = makeDemoProject();
    PlaybackSnapshot s;
    REQUIRE(compilePlayback(p, p.patterns[0].id, false, s));
    REQUIRE(s.loopEnd == 8);
    REQUIRE(s.noteCount == p.patterns[0].notes.size());
    REQUIRE(compilePlayback(p, p.patterns[0].id, true, s));
    for (int i = 0; i < 60; ++i)
    {
        auto c = p.tracks[0].clips[0];
        c.id = ClipID{p.nextId()};
        c.startBeat = i * 8;
        p.tracks[0].clips.push_back(c);
    }
    REQUIRE_FALSE(compilePlayback(p, p.patterns[0].id, true, s));
}
TEST_CASE("ADSR releases to silence", "[dsp]")
{
    Envelope e;
    e.prepare(1000);
    e.set(0.01f, 0.01f, 0.5f, 0.01f);
    e.on();
    for (int i = 0; i < 40; ++i)
        e.next();
    REQUIRE(e.next() == Catch::Approx(0.5f));
    e.off();
    for (int i = 0; i < 12; ++i)
        e.next();
    REQUIRE_FALSE(e.active());
    REQUIRE(e.next() == 0);
}
TEST_CASE("Delay has expected impulse and finite feedback", "[dsp]")
{
    Delay d;
    d.prepare(1000);
    REQUIRE(d.process(1, 1) == 1);
    for (int i = 1; i < 375; ++i)
        REQUIRE(d.process(0, 1) == 0);
    REQUIRE(d.process(0, 1) == 1);
    for (int i = 0; i < 2000; ++i)
        REQUIRE(std::isfinite(d.process(0, 1)));
}
TEST_CASE("Voice stealing is bounded and all notes off clears voices", "[voices]")
{
    Synth s;
    s.prepare(48000);
    for (int i = 0; i < 100; ++i)
        s.noteOn(0, i % 128, 0.8f, InstrumentType::Synth);
    REQUIRE(s.activeVoices() == 64);
    s.allNotesOff();
    REQUIRE(s.activeVoices() == 0);
}
TEST_CASE("Realtime renderer loops across arbitrary block sizes", "[engine]")
{
    auto r = std::make_unique<RenderEngine>();
    r->prepare(48000);
    PlaybackSnapshot p;
    p.bpm = 120;
    p.channelCount = 1;
    p.loopEnd = 0.01;
    p.noteCount = 1;
    p.notes[0] = {0, 60, 0, 0.009, 1};
    REQUIRE(r->publish(p));
    r->setPlaying(true);
    std::array<float, 512> l{}, right{};
    r->render(l.data(), right.data(), 512);
    REQUIRE(r->positionBeat() == Catch::Approx(32.0 / 24000));
    REQUIRE(r->overflows() == 0);
    bool audible = false;
    for (auto x : l)
    {
        REQUIRE(std::isfinite(x));
        audible |= std::abs(x) > 0.0001f;
    }
    REQUIRE(audible);
}
TEST_CASE("Mute and solo produce silence for excluded channels", "[mixer]")
{
    auto p = makeDemoProject();
    for (auto &c : p.channels)
        c.muted = true;
    PlaybackSnapshot s;
    REQUIRE(compilePlayback(p, p.patterns[0].id, true, s));
    auto r = std::make_unique<RenderEngine>();
    r->prepare(48000);
    r->publish(s);
    r->setPlaying(true);
    std::array<float, 512> l{}, right{};
    r->render(l.data(), right.data(), 512);
    for (auto x : l)
        REQUIRE(x == 0);
    p.channels[0].muted = false;
    p.channels[0].soloed = true;
    REQUIRE(compilePlayback(p, p.patterns[0].id, true, s));
    REQUIRE(s.channels[0].audible);
    REQUIRE_FALSE(s.channels[1].audible);
}
TEST_CASE("Automation interpolates and clamps", "[automation]")
{
    std::vector<AutomationPoint> a{{0, 0}, {4, 1}};
    REQUIRE(automationAt(a, 2) == 0.5f);
    REQUIRE(automationAt(a, -1) == 0);
    REQUIRE(automationAt(a, 8) == 1);
}
TEST_CASE("Binary project roundtrip and corruption are transactional", "[persistence]")
{
    auto file = std::filesystem::temp_directory_path() / "forge-catch-roundtrip.odaw";
    auto p = makeDemoProject();
    REQUIRE(saveProject(p, file));
    Project loaded;
    REQUIRE(loadProject(file, loaded));
    REQUIRE(loaded == p);
    REQUIRE(saveProject(p, file));
    {
        std::fstream f(file, std::ios::binary | std::ios::in | std::ios::out);
        f.seekg(40);
        char original = 0;
        f.get(original);
        f.seekp(40);
        f.put(static_cast<char>(original ^ 0x55));
    }
    REQUIRE_FALSE(loadProject(file, loaded));
    REQUIRE(loaded == p);
    std::filesystem::remove(file);
}
TEST_CASE("SPSC FIFO overflow and concurrent ordering", "[queue]")
{
    SpscQueue<int, 4> small;
    REQUIRE(small.push(1));
    REQUIRE(small.push(2));
    REQUIRE(small.push(3));
    REQUIRE_FALSE(small.push(4));
    int value = 0;
    REQUIRE(small.pop(value));
    REQUIRE(value == 1);
    SpscQueue<int, 256> q;
    std::thread producer(
        [&]
        {
            for (int i = 0; i < 10000; ++i)
                while (!q.push(i))
                    std::this_thread::yield();
        });
    for (int i = 0; i < 10000; ++i)
    {
        while (!q.pop(value))
            std::this_thread::yield();
        REQUIRE(value == i);
    }
    producer.join();
}

TEST_CASE("Sampler interpolation and pitch playback", "[sampler]")
{
    SampleData sample;
    sample.mono = {0, 1, 0, -1, 0};
    REQUIRE(sample.read(0.5) == 0.5f);
    REQUIRE(sample.read(3.5) == -0.5f);
    REQUIRE(sample.read(9) == 0);
    Synth synth;
    synth.prepare(48000);
    sample.mono.resize(48000, 0.25f);
    synth.setSample(0, std::move(sample));
    PlaybackSnapshot p;
    p.channelCount = 1;
    p.channels[0].instrument = InstrumentType::Sampler;
    synth.noteOn(0, 60, 1, InstrumentType::Sampler);
    float l = 0, r = 0;
    bool sound = false;
    for (int i = 0; i < 512; ++i)
    {
        synth.renderSample(p, l, r);
        sound |= std::abs(l) > 0.001f;
    }
    REQUIRE(sound);
}

TEST_CASE("MIDI loop recording closes held notes and preserves velocity", "[midi]")
{
    MidiRecorder r;
    r.begin(4, 2);
    r.receive({2, 60, 0.75f, true, 3.5});
    r.receive({2, 60, 0, false, 0.5});
    auto notes = r.finish(1);
    REQUIRE(notes.size() == 1);
    REQUIRE(notes[0].startBeat == 3.5);
    REQUIRE(notes[0].lengthBeats == 1);
    REQUIRE(notes[0].velocity == 0.75f);
    r.begin(4, 2);
    r.receive({2, 62, 0.5f, true, 1});
    notes = r.finish(2);
    REQUIRE(notes[0].lengthBeats == 1);
}

TEST_CASE("Pause preserves position and rewind resets it", "[engine]")
{
    auto r = std::make_unique<RenderEngine>();
    r->prepare(48000);
    PlaybackSnapshot p;
    p.channelCount = 1;
    r->publish(p);
    r->setPlaying(true);
    std::array<float, 64> l{}, right{};
    r->render(l.data(), right.data(), 64);
    const auto beat = r->positionBeat();
    r->setPlaying(false);
    r->render(l.data(), right.data(), 64);
    REQUIRE(r->positionBeat() == beat);
    r->rewind();
    r->render(l.data(), right.data(), 64);
    REQUIRE(r->positionBeat() == 0);
}
TEST_CASE("Empty tiny patterns cannot make compilation unbounded", "[model]")
{
    auto p = makeDemoProject();
    p.patterns[0].notes.clear();
    p.patterns[0].lengthBeats = 1e-100;
    REQUIRE_FALSE(p.validate());
}

TEST_CASE("JSON debug export escapes text", "[persistence]")
{
    auto p = makeDemoProject();
    p.metadata.name = "A \"quote\"\nline";
    const auto json = projectJson(p);
    REQUIRE(json.find("A \\\"quote\\\"\\nline") != std::string::npos);
    REQUIRE(json.find("\"patterns\"") != std::string::npos);
}

TEST_CASE("Reverb impulse has a decaying finite tail", "[dsp]")
{
    Reverb r;
    r.prepare(48000);
    r.process(1, 1);
    double early = 0, late = 0;
    for (int i = 1; i < 192000; ++i)
    {
        const auto v = r.process(0, 1);
        REQUIRE(std::isfinite(v));
        if (i < 48000)
            early += v * v;
        if (i > 144000)
            late += v * v;
    }
    REQUIRE(early > 0);
    REQUIRE(late < early * 0.01);
}
TEST_CASE("Compressor attenuates above threshold and supports sidechain", "[dsp]")
{
    Compressor c;
    c.prepare(48000);
    float out = 0;
    for (int i = 0; i < 48000; ++i)
        out = c.process(0.8f, 0.8f, -18);
    REQUIRE(std::abs(out) < 0.3f);
    c.reset();
    for (int i = 0; i < 48000; ++i)
        out = c.process(0.1f, 1, -18);
    REQUIRE(out < 0.05f);
}
TEST_CASE("Eight band EQ is neutral at zero gain and boosts its centre", "[dsp]")
{
    EQ eq;
    eq.prepare(48000);
    for (int i = 0; i < 512; ++i)
    {
        float v = static_cast<float>(std::sin(i * 0.1));
        REQUIRE(eq.process(v) == Catch::Approx(v).margin(1e-6));
    }
    eq.setBand(4, 1000, 6, 1);
    double energy = 0;
    for (int i = 0; i < 48000; ++i)
    {
        const auto v = eq.process(static_cast<float>(std::sin(2 * pi * 1000 * i / 48000)));
        REQUIRE(std::isfinite(v));
        if (i > 24000)
            energy += v * v;
    }
    REQUIRE(energy > 24000 * 0.5 * 3.8);
}

TEST_CASE("Automation snapshot applies interpolation to channel parameters", "[automation]")
{
    auto p = makeDemoProject();
    p.automation[0].interpolation = Interpolation::Linear;
    PlaybackSnapshot s;
    REQUIRE(compilePlayback(p, p.patterns[0].id, true, s));
    applyAutomation(s, 4);
    REQUIRE(s.channels[3].cutoff == Catch::Approx(0.385f));
    p.automation[0].interpolation = Interpolation::Step;
    REQUIRE(compilePlayback(p, p.patterns[0].id, true, s));
    applyAutomation(s, 4);
    REQUIRE(s.channels[3].cutoff == Catch::Approx(0.12f));
}

TEST_CASE("Plugin slots roundtrip opaque state and bypass", "[persistence][plugins]")
{
    auto p = makeDemoProject();
    p.masterInserts.push_back(
        {PluginInstanceID{p.nextId()}, "Fixture", "<PLUGIN name=\"Fixture\"/>", {0, 255, 1, 128}, true, {}});
    p.masterInserts[0].normalizedParameters = {0.25f, 1.0f};
    auto file = std::filesystem::temp_directory_path() / "forge-plugin-roundtrip.odaw";
    REQUIRE(saveProject(p, file));
    Project loaded;
    REQUIRE(loadProject(file, loaded));
    REQUIRE(loaded == p);
    std::filesystem::remove(file);
    p.masterInserts[0].state.resize(4 * 1024 * 1024 + 1);
    REQUIRE_FALSE(p.validate());
}
TEST_CASE("Version three sessions migrate without plugin inserts", "[persistence][migration]")
{
    Project loaded;
    REQUIRE(loadProject(std::filesystem::path(FORGE_TEST_SOURCE_DIR) / "fixtures/legacy-v3.odaw", loaded));
    REQUIRE(loaded.validate());
    REQUIRE(loaded.masterInserts.empty());
    REQUIRE(loaded.channels.size() == 6);
}

TEST_CASE("Note audition sounds while paused without advancing or recording", "[engine][preview]")
{
    auto renderer = std::make_unique<RenderEngine>();
    renderer->prepare(48000);
    PlaybackSnapshot state;
    state.channelCount = 1;
    REQUIRE(renderer->publish(state));
    renderer->setRecording(true);
    REQUIRE(renderer->midi({72, 0.8f, true, 0, false}));
    std::array<float, 512> left{}, right{};
    renderer->render(left.data(), right.data(), 512);
    double energy = 0;
    for (float value : left)
        energy += value * value;
    REQUIRE(energy > 0.001);
    REQUIRE_FALSE(renderer->playing());
    REQUIRE(renderer->positionBeat() == 0);
    CapturedMidi captured;
    REQUIRE_FALSE(renderer->capture(captured));
    REQUIRE(renderer->midi({72, 0, false, 0, false}));
    for (int block = 0; block < 100; ++block)
        renderer->render(left.data(), right.data(), 512);
    for (float value : left)
        REQUIRE(std::abs(value) < 0.000001f);
    REQUIRE_FALSE(renderer->capture(captured));
    REQUIRE(renderer->midi({60, 0.7f, true, 0}));
    renderer->render(left.data(), right.data(), 512);
    REQUIRE(renderer->capture(captured));
    REQUIRE(captured.pitch == 60);
    REQUIRE(captured.on);
    REQUIRE(renderer->midi({60, 0, false, -1, false}));
    REQUIRE(renderer->midi({60, 1, true, static_cast<int>(maxChannels), false}));
    renderer->render(left.data(), right.data(), 512);
}

TEST_CASE("Preview release leaves a playing note of the same pitch intact", "[preview]")
{
    auto synth = std::make_unique<Synth>();
    synth->prepare(48000);
    PlaybackSnapshot state;
    state.channelCount = 1;
    synth->noteOn(0, 60, 0.7f, InstrumentType::Synth);
    synth->noteOn(0, 60, 0.7f, InstrumentType::Synth, true);
    float left = 0, right = 0;
    for (int i = 0; i < 512; ++i)
        synth->renderSample(state, left, right);
    REQUIRE(synth->activeVoices() == 2);
    synth->noteOff(0, 60, true);
    for (int i = 0; i < 24000; ++i)
        synth->renderSample(state, left, right);
    REQUIRE(synth->activeVoices() == 1);
    synth->noteOff(0, 60);
    for (int i = 0; i < 24000; ++i)
        synth->renderSample(state, left, right);
    REQUIRE(synth->activeVoices() == 0);
}

TEST_CASE("Factory restoration can retain beat position and explicit rewind wins", "[engine]")
{
    auto renderer = std::make_unique<RenderEngine>();
    renderer->prepare(48000);
    PlaybackSnapshot snapshot;
    snapshot.channelCount = 1;
    REQUIRE(renderer->publish(snapshot));
    renderer->seekBeat(7);
    renderer->prepare(44100);
    std::array<float, 64> left{}, right{};
    renderer->render(left.data(), right.data(), 64);
    REQUIRE(renderer->positionBeat() == Catch::Approx(7));
    renderer->seekBeat(9);
    renderer->rewind();
    renderer->render(left.data(), right.data(), 64);
    REQUIRE(renderer->positionBeat() == 0);
}
