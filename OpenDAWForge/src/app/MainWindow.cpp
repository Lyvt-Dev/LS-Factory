#include "MainWindow.h"
#include "persistence/BinaryFormat.h"
#include "persistence/JsonExporter.h"
#include <algorithm>
#include <cmath>
namespace forge
{
namespace
{
const juce::Colour background{0xff222a2f}, panel{0xff303a40}, raised{0xff414d54}, grid{0xff293940},
    ink{0xffd5dddf}, muted{0xff8c9da4}, accent{0xffc4dc80};
void text(juce::Graphics &g, const juce::String &s, juce::Rectangle<int> b, float size = 12,
          juce::Colour color = ink, int align = juce::Justification::centredLeft)
{
    g.setColour(color);
    g.setFont(juce::Font(juce::FontOptions(size)));
    g.drawText(s, b, align, true);
}
void box(juce::Graphics &g, juce::Rectangle<int> b, juce::Colour c)
{
    g.setColour(c);
    g.fillRect(b);
    g.setColour(c.brighter(0.12f));
    g.drawRect(b, 1);
}
const char *noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
juce::String pitchName(int pitch)
{
    return juce::String(noteNames[pitch % 12]) + juce::String(pitch / 12 - 1);
}
} // namespace
Workspace::Workspace(bool device)
{
    setLookAndFeel(&look_);
    look_.setColour(juce::TextButton::buttonColourId, raised);
    look_.setColour(juce::TextButton::textColourOffId, ink);
    look_.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.5f));
    selected_ = project_.patterns.front().id;
    for (auto *button :
         {&play_, &stop_, &song_, &loop_, &save_, &open_, &export_, &settings_, &playlist_, &piano_, &mixer_,
          &rackButton_, &undoButton_, &redoButton_, &midiRecord_, &audioRecord_, &pluginsButton_})
        addAndMakeVisible(button);
    play_.onClick = [this] { transportToggle(); };
    stop_.onClick = [this]
    {
        audio_.renderer.setPlaying(false);
        audio_.renderer.rewind();
        sync();
    };
    song_.onClick = [this]
    {
        songMode_ = !songMode_;
        song_.setButtonText(songMode_ ? "SONG" : "PATTERN");
        audio_.renderer.rewind();
        sync();
    };
    loop_.onClick = [this] { edit("Toggle loop", [](Project &p) { p.loopEnabled = !p.loopEnabled; }); };
    save_.onClick = [this] { chooseSave(); };
    open_.onClick = [this] { chooseOpen(); };
    export_.onClick = [this] { chooseExport(); };
    pluginsButton_.onClick = [this] { showPlugins(); };
    settings_.onClick = [this] { showSettings(); };
    midiRecord_.onClick = [this] { toggleMidiRecording(); };
    audioRecord_.onClick = [this] { toggleAudioRecording(); };
    playlist_.onClick = [this] { selectView(View::Playlist); };
    piano_.onClick = [this] { selectView(View::Piano); };
    mixer_.onClick = [this] { selectView(View::Mixer); };
    rackButton_.onClick = [this]
    {
        showRack_ = !showRack_;
        resized();
        repaint();
    };
    undoButton_.onClick = [this]
    {
        if (!pluginsLoading_ && undo_.undo(project_))
        {
            dirty_ = true;
            sync(true);
            restoreSamplesAsync();
        }
    };
    redoButton_.onClick = [this]
    {
        if (!pluginsLoading_ && undo_.redo(project_))
        {
            dirty_ = true;
            sync(true);
            restoreSamplesAsync();
        }
    };
    tempo_.setSliderStyle(juce::Slider::IncDecButtons);
    tempo_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 62, 28);
    tempo_.setRange(20, 400, 0.1);
    tempo_.setValue(115);
    tempo_.setColour(juce::Slider::textBoxBackgroundColourId, background);
    tempo_.onValueChange = [this]
    {
        const double bpm = tempo_.getValue();
        edit("Set tempo", [bpm](Project &p) { p.tempoMap.points = {{0, bpm}}; });
    };
    addAndMakeVisible(tempo_);
    patterns_.onChange = [this]
    {
        const int index = patterns_.getSelectedItemIndex();
        if (index >= 0 && index < static_cast<int>(project_.patterns.size()))
        {
            selected_ = project_.patterns[static_cast<std::size_t>(index)].id;
            audio_.renderer.rewind();
            sync();
        }
    };
    addAndMakeVisible(patterns_);
    channels_.onChange = [this]
    {
        selectedChannel_ = std::max(0, channels_.getSelectedItemIndex());
        audio_.renderer.setMidiChannel(selectedChannel_);
        repaint();
    };
    addAndMakeVisible(channels_);
    search_.setTextToShowWhenEmpty("Search instruments / patterns", muted);
    search_.onTextChange = [this] { repaint(); };
    search_.setColour(juce::TextEditor::backgroundColourId, background);
    addAndMakeVisible(search_);
    play_.setTooltip("Space: play / pause");
    midiRecord_.setTooltip("Record MIDI into the selected pattern");
    audioRecord_.setTooltip("Record a configured audio input to WAV");
    rackButton_.setTooltip("F6: show / hide Channel Rack");
    export_.setTooltip("Render the arrangement to 24-bit, 48 kHz stereo WAV");
    setWantsKeyboardFocus(true);
    setSize(1440, 900);
    if (device)
    {
        const auto error = audio_.initialise();
        if (error.isNotEmpty())
            message_ = audio_.status + juce::String::fromUTF8(" · Offline WAV export available");
    }
    else
        audio_.renderer.prepare(48000);
    sync(true);
    startTimerHz(30);
}
Workspace::~Workspace()
{
    stopTimer();
    closePluginEditor();
    workers_.removeAllJobs(true, -1);
    setLookAndFeel(nullptr);
}
Pattern *Workspace::activePattern()
{
    return project_.findPattern(selected_);
}
const Pattern *Workspace::activePattern() const
{
    return project_.findPattern(selected_);
}
void Workspace::sync(bool refresh)
{
    audio_.pluginTempo.store(project_.tempoMap.bpmAt(0));
    if (refresh)
    {
        patterns_.clear(juce::dontSendNotification);
        int i = 1;
        for (const auto &p : project_.patterns)
        {
            patterns_.addItem(p.name, i);
            if (p.id == selected_)
                patterns_.setSelectedId(i, juce::dontSendNotification);
            ++i;
        }
        if (!activePattern() && !project_.patterns.empty())
        {
            selected_ = project_.patterns.front().id;
            patterns_.setSelectedId(1, juce::dontSendNotification);
        }
        channels_.clear(juce::dontSendNotification);
        i = 1;
        for (const auto &c : project_.channels)
            channels_.addItem(c.name, i++);
        selectedChannel_ =
            std::clamp(selectedChannel_, 0, std::max(0, static_cast<int>(project_.channels.size()) - 1));
        channels_.setSelectedId(selectedChannel_ + 1, juce::dontSendNotification);
        tempo_.setValue(project_.tempoMap.bpmAt(0), juce::dontSendNotification);
    }
    PlaybackSnapshot snapshot;
    const auto result = compilePlayback(project_, selected_, songMode_, snapshot);
    if (!result)
        message_ = result.error;
    else
    {
        pendingSnapshot_ = snapshot;
        pendingAudio_ = !audio_.renderer.publish(snapshot);
    }
    play_.setToggleState(audio_.renderer.playing(), juce::dontSendNotification);
    loop_.setToggleState(project_.loopEnabled, juce::dontSendNotification);
    song_.setToggleState(songMode_, juce::dontSendNotification);
    undoButton_.setEnabled(undo_.canUndo());
    redoButton_.setEnabled(undo_.canRedo());
    repaint();
}
void Workspace::edit(const std::string &description, const std::function<void(Project &)> &change)
{
    if (pluginsLoading_)
    {
        message_ = "Wait for plugin loading to finish";
        return;
    }
    const auto result = undo_.execute(project_, description, change);
    if (!result)
        message_ = result.error;
    else
    {
        dirty_ = true;
        message_ = description;
        sync();
    }
}
void Workspace::transportToggle()
{
    audio_.renderer.setPlaying(!audio_.renderer.playing());
    play_.setButtonText(audio_.renderer.playing() ? "PAUSE" : "PLAY");
    play_.setToggleState(audio_.renderer.playing(), juce::dontSendNotification);
    repaint();
}
void Workspace::selectView(View v)
{
    view_ = v;
    if (v == View::Piano)
    {
        const auto *p = activePattern();
        if (p)
        {
            for (const auto &n : p->notes)
                if (n.channel == selectedChannel_)
                {
                    pianoTop_ = std::clamp(n.pitch + 12, 24, 127);
                    break;
                }
        }
    }
    resized();
    repaint();
}
void Workspace::timerCallback()
{
    if (pluginEdited_.exchange(false))
        dirty_ = true;
    if (pendingAudio_)
        pendingAudio_ = !audio_.renderer.publish(pendingSnapshot_);
    CapturedMidi event;
    while (audio_.renderer.capture(event))
        midiRecorder_.receive(event);
    repaint();
    if (++autosaveTicks_ >= 900)
    {
        autosaveTicks_ = 0;
        if (dirty_)
            saveTo(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("LSFactory/recovery.odaw"),
                   true);
    }
}
juce::Rectangle<int> Workspace::timeline() const
{
    return editorBounds_.withTrimmedLeft(view_ == View::Piano ? 64 : 158)
        .withTrimmedTop(48)
        .withTrimmedBottom(view_ == View::Piano ? 76 : 0);
}
void Workspace::resized()
{
    auto row = juce::Rectangle<int>(244, 12, getWidth() - 258, 32);
    auto place = [&](juce::Component &c, int w) { c.setBounds(row.removeFromLeft(w).reduced(2)); };
    place(song_, 72);
    place(play_, 65);
    place(stop_, 60);
    place(loop_, 60);
    place(tempo_, 112);
    row.removeFromLeft(150);
    place(open_, 62);
    place(save_, 62);
    place(export_, 108);
    place(settings_, 68);
    place(midiRecord_, 80);
    place(audioRecord_, 68);
    place(pluginsButton_, 76);
    auto tabs = juce::Rectangle<int>(244, 62, getWidth() - 258, 27);
    auto tab = [&](juce::Component &c, int w) { c.setBounds(tabs.removeFromLeft(w).reduced(2, 0)); };
    tab(playlist_, 114);
    tab(piano_, 124);
    tab(mixer_, 108);
    tab(rackButton_, 150);
    tab(patterns_, 156);
    tab(channels_, 156);
    tab(undoButton_, 58);
    tab(redoButton_, 58);
    browserBounds_ = {8, 100, 220, getHeight() - 136};
    search_.setBounds(18, 138, 200, 28);
    editorBounds_ = {238, 100, getWidth() - 246, getHeight() - 136};
    rackBounds_ = {268, getHeight() - 296, std::min(780, getWidth() - 296), 246};
    if (showRack_ && view_ != View::Playlist)
        editorBounds_.setBottom(rackBounds_.getY() - 10);
    playlist_.setToggleState(view_ == View::Playlist, juce::dontSendNotification);
    piano_.setToggleState(view_ == View::Piano, juce::dontSendNotification);
    mixer_.setToggleState(view_ == View::Mixer, juce::dontSendNotification);
    rackButton_.setToggleState(showRack_, juce::dontSendNotification);
}
void Workspace::paint(juce::Graphics &g)
{
    g.fillAll(background);
    box(g, {0, 0, getWidth(), 94}, panel);
    text(g, "LS", {18, 11, 42, 37}, 28, accent);
    text(g, "FACTORY", {60, 14, 130, 30}, 21);
    text(g, "PATTERN WORKSTATION", {20, 53, 205, 18}, 10, muted);
    const double beat = audio_.renderer.positionBeat();
    const int bar = static_cast<int>(beat / 4) + 1;
    const int subdivision = static_cast<int>(std::fmod(beat, 4)) + 1;
    text(g,
         juce::String(bar).paddedLeft('0', 3) + ":" + juce::String(subdivision).paddedLeft('0', 2) + ":" +
             juce::String(static_cast<int>(std::fmod(beat, 1) * 100)).paddedLeft('0', 2),
         {626, 10, 132, 36}, 25, accent);
    drawBrowser(g);
    if (view_ == View::Playlist)
        drawPlaylist(g);
    else if (view_ == View::Piano)
        drawPiano(g);
    else
        drawMixer(g);
    if (showRack_)
        drawRack(g);
    box(g, {0, getHeight() - 29, getWidth(), 29}, panel);
    text(g, message_, {14, getHeight() - 27, getWidth() - 370, 24}, 11, muted);
    auto *device = audio_.devices.getCurrentAudioDevice();
    const juce::String deviceInfo =
        device ? juce::String(device->getCurrentSampleRate() / 1000, 1) + juce::String::fromUTF8(" kHz · ") +
                     juce::String(device->getCurrentBufferSizeSamples()) + " samples"
               : "Offline audio";
    text(g, deviceInfo + "   CPU " + juce::String(audio_.cpuPercent.load(), 1) + "%",
         {getWidth() - 350, getHeight() - 27, 337, 24}, 11, accent, juce::Justification::centredRight);
}
void Workspace::drawBrowser(juce::Graphics &g)
{
    box(g, browserBounds_, panel.darker(0.16f));
    text(g, "BROWSER", browserBounds_.withHeight(30).reduced(10, 0), 11, muted);
    g.setColour(raised);
    g.drawHorizontalLine(132, 18, 218);
    int y = 184;
    auto heading = [&](const juce::String &title)
    {
        text(g, title, {20, y, 195, 26}, 11, muted);
        y += 30;
    };
    auto item = [&](const juce::String &title, juce::Colour c)
    {
        if (search_.getText().isNotEmpty() && !title.containsIgnoreCase(search_.getText()))
            return;
        g.setColour(c);
        g.fillRoundedRectangle(26.0f, static_cast<float>(y + 9), 6, 6, 1);
        text(g, title, {42, y, 166, 24}, 12);
        y += 26;
    };
    heading(juce::String::fromUTF8("▾  CURRENT PROJECT"));
    item(displayProject().metadata.name, accent);
    item("Pattern library", muted);
    heading(juce::String::fromUTF8("▾  NATIVE INSTRUMENTS"));
    item(juce::String::fromUTF8("Forge · Subtractive synth"), juce::Colour(0xffb89ac9));
    item(juce::String::fromUTF8("Pulse · Drum synthesis"), juce::Colour(0xff9dbd77));
    heading(juce::String::fromUTF8("▾  CHANNELS"));
    for (const auto &c : displayProject().channels)
        item(c.name, juce::Colour(c.color));
    heading(juce::String::fromUTF8("▾  PATTERNS"));
    for (const auto &p : displayProject().patterns)
        item(p.name, juce::Colour(p.color));
    text(g, "SESSION 01", {20, browserBounds_.getBottom() - 68, 180, 22}, 10, muted);
    text(g, dirty_ ? juce::String::fromUTF8("● Unsaved changes") : juce::String::fromUTF8("● Session loaded"),
         {20, browserBounds_.getBottom() - 43, 180, 22}, 11, accent);
}
void Workspace::drawPlaylist(juce::Graphics &g)
{
    box(g, editorBounds_, panel);
    text(g, "PLAYLIST  /  " + displayProject().metadata.name, editorBounds_.withHeight(28).reduced(10, 0),
         12);
    text(g, juce::String::fromUTF8("DRAW · Right-click erase · Drag clips · Ctrl + wheel zoom"),
         editorBounds_.withHeight(28).reduced(10, 0), 10, muted, juce::Justification::centredRight);
    const auto area = timeline();
    g.setColour(grid);
    g.fillRect(area);
    for (int b = 0; b * beatWidth_ < area.getWidth(); ++b)
    {
        const int x = area.getX() + static_cast<int>(b * beatWidth_);
        g.setColour(b % 4 == 0 ? raised : grid.brighter(0.08f));
        g.drawVerticalLine(x, static_cast<float>(area.getY() - 20), static_cast<float>(area.getBottom()));
        if (b % 4 == 0)
            text(g, juce::String(b / 4 + 1), {x + 4, area.getY() - 21, 40, 19}, 10, muted);
    }
    const float loopX = area.getX() + static_cast<float>(displayProject().loopStart) * beatWidth_;
    const float loopW =
        static_cast<float>(displayProject().loopEnd - displayProject().loopStart) * beatWidth_;
    g.setColour(accent.withAlpha(0.25f));
    g.fillRect(loopX, static_cast<float>(area.getY() - 23), loopW, 3.0f);
    const int trackHeight = 72;
    const int visible = std::max(8, static_cast<int>(displayProject().tracks.size()));
    for (int index = 0; index < visible; ++index)
    {
        int y = area.getY() + index * trackHeight;
        if (y >= area.getBottom())
            break;
        g.setColour(raised);
        g.drawHorizontalLine(y, static_cast<float>(editorBounds_.getX()),
                             static_cast<float>(area.getRight()));
        if (index >= static_cast<int>(displayProject().tracks.size()))
        {
            text(g, "Track " + juce::String(index + 1), {editorBounds_.getX() + 12, y + 6, 140, 25}, 11,
                 muted);
            continue;
        }
        const auto &t = displayProject().tracks[static_cast<std::size_t>(index)];
        g.setColour(juce::Colour(t.color));
        g.fillRect(editorBounds_.getX() + 1, y + 1, 4, trackHeight - 1);
        text(g, t.name, {editorBounds_.getX() + 14, y + 4, 138, 26}, 12);
        text(g, "PATTERN", {editorBounds_.getX() + 14, y + 33, 130, 20}, 9, muted);
        g.saveState();
        g.reduceClipRegion(area.withY(y).withHeight(trackHeight));
        for (std::size_t ci = 0; ci < t.clips.size(); ++ci)
        {
            const auto &c = t.clips[ci];
            auto r = juce::Rectangle<int>(area.getX() + static_cast<int>(c.startBeat * beatWidth_), y + 4,
                                          std::max(4, static_cast<int>(c.lengthBeats * beatWidth_) - 2),
                                          trackHeight - 9);
            auto color = juce::Colour(t.color).withMultipliedBrightness(c.muted ? 0.45f : 0.8f);
            box(g, r, color.darker(0.3f));
            text(g, displayProject().findPattern(c.patternId)->name, r.withHeight(22).reduced(5, 0), 11, ink);
            const auto *pat = displayProject().findPattern(c.patternId);
            for (const auto &n : pat->notes)
            {
                const float nx = r.getX() + static_cast<float>(n.startBeat) * beatWidth_;
                const float ny = r.getY() + 34 + static_cast<float>(n.pitch % 24) * 0.7f;
                g.setColour(color.brighter(0.35f));
                g.fillRect(nx, ny, std::max(3.0f, static_cast<float>(n.lengthBeats) * beatWidth_), 2.0f);
            }
        }
        g.restoreState();
    }
    for (std::size_t i = 0; i < displayProject().automation.size(); ++i)
    {
        const auto &lane = displayProject().automation[i];
        const int y = area.getY() + static_cast<int>(displayProject().tracks.size() + i) * trackHeight;
        if (y >= area.getBottom())
            break;
        box(g, {editorBounds_.getX() + 1, y + 1, 156, trackHeight - 1}, juce::Colour(0xff6d6252));
        text(g, lane.name, {editorBounds_.getX() + 12, y + 5, 140, 28}, 11);
        box(g,
            {area.getX(), y + 4,
             std::min(area.getWidth(), static_cast<int>(displayProject().loopEnd * beatWidth_)),
             trackHeight - 9},
            juce::Colour(0xff706953));
        juce::Path path;
        for (std::size_t j = 0; j < lane.points.size(); ++j)
        {
            const auto &point = lane.points[j];
            const float x = area.getX() + static_cast<float>(point.beat) * beatWidth_,
                        py = y + 61 - point.value * 48;
            if (j == 0)
                path.startNewSubPath(x, py);
            else
                path.lineTo(x, py);
            g.setColour(accent);
            g.fillEllipse(x - 3, py - 3, 6, 6);
        }
        g.setColour(accent.withAlpha(0.8f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    if (audio_.renderer.playing())
    {
        int x = area.getX() + static_cast<int>(audio_.renderer.positionBeat() * beatWidth_);
        g.setColour(accent);
        g.drawVerticalLine(x, static_cast<float>(area.getY() - 20), static_cast<float>(area.getBottom()));
    }
}
void Workspace::drawPiano(juce::Graphics &g)
{
    box(g, editorBounds_, panel);
    const auto *pat = activePattern();
    text(g, "PIANO ROLL  /  " + (pat ? juce::String(pat->name) : ""),
         editorBounds_.withHeight(28).reduced(10, 0), 12);
    text(g, juce::String::fromUTF8("Click draw · Drag move / right edge resize · Right-click erase"),
         editorBounds_.withHeight(28).reduced(10, 0), 10, muted, juce::Justification::centredRight);
    const auto area = timeline();
    const int row = 18;
    g.setColour(grid);
    g.fillRect(area);
    for (int i = 0; i < area.getHeight() / row; ++i)
    {
        const int pitch = pianoTop_ - i;
        if (pitch < 0)
            break;
        const int y = area.getY() + i * row;
        const int key = pitch % 12;
        const bool black = key == 1 || key == 3 || key == 6 || key == 8 || key == 10;
        box(g, {editorBounds_.getX() + 1, y, 62, row}, black ? background : juce::Colour(0xffb7c0bf));
        text(g, pitchName(pitch), {editorBounds_.getX() + 8, y, 49, row}, 9, black ? muted : background,
             juce::Justification::centredRight);
        g.setColour(black ? grid.darker(0.09f) : grid.brighter(0.03f));
        g.fillRect(area.getX(), y, area.getWidth(), row - 1);
    }
    const float scale = beatWidth_ * 2;
    for (int b = 0; b * scale / 4 < area.getWidth(); ++b)
    {
        const int x = area.getX() + static_cast<int>(b * scale / 4);
        g.setColour(b % 16 == 0 ? raised.brighter(0.2f) : b % 4 == 0 ? raised : grid.brighter(0.07f));
        g.drawVerticalLine(x, static_cast<float>(area.getY() - 20),
                           static_cast<float>(area.getBottom() + 70));
        if (b % 16 == 0)
            text(g, juce::String(b / 16 + 1), {x + 4, area.getY() - 20, 30, 18}, 10, muted);
    }
    text(g, "VELOCITY", {editorBounds_.getX() + 4, area.getBottom() + 6, 61, 22}, 9, muted);
    if (pat)
    {
        g.saveState();
        g.reduceClipRegion(area.withHeight(area.getHeight() + 72));
        for (const auto &n : pat->notes)
        {
            const int x = area.getX() + static_cast<int>(n.startBeat * scale);
            const int y = area.getY() + (pianoTop_ - n.pitch) * row;
            const auto c = juce::Colour(displayProject().channels[static_cast<std::size_t>(n.channel)].color);
            const bool active = n.channel == selectedChannel_;
            box(g, {x, y + 1, std::max(5, static_cast<int>(n.lengthBeats * scale) - 1), row - 2},
                active ? c : c.withAlpha(0.2f));
            if (active)
            {
                text(g, pitchName(n.pitch), {x + 3, y + 1, 40, row - 2}, 9, background);
                g.setColour(c);
                const int h = static_cast<int>(n.velocity * 48);
                g.fillRect(x, area.getBottom() + 62 - h, 3, h);
            }
        }
        g.restoreState();
    }
    if (audio_.renderer.playing())
    {
        g.setColour(accent);
        g.drawVerticalLine(area.getX() + static_cast<int>(audio_.renderer.positionBeat() * scale),
                           static_cast<float>(area.getY()), static_cast<float>(area.getBottom() + 66));
    }
}
void Workspace::drawMixer(juce::Graphics &g)
{
    box(g, editorBounds_, panel);
    text(g, juce::String::fromUTF8("MIXER  /  Click fader · Right-click pan · Mute / solo"),
         editorBounds_.withHeight(28).reduced(10, 0), 12);
    const int top = editorBounds_.getY() + 55, bottom = editorBounds_.getBottom() - 44;
    const int count = static_cast<int>(displayProject().channels.size()) + 1;
    for (int i = 0; i < count; ++i)
    {
        const int x = editorBounds_.getX() + 16 + i * 92;
        const bool master = i == 0;
        const auto color =
            master ? accent : juce::Colour(displayProject().channels[static_cast<std::size_t>(i - 1)].color);
        box(g, {x, top, 84, bottom - top + 32}, background.brighter(0.03f));
        g.setColour(color);
        g.fillRect(x + 1, top + 1, 82, 4);
        text(g,
             master ? "MASTER"
                    : juce::String(displayProject().channels[static_cast<std::size_t>(i - 1)].name),
             {x + 5, top + 12, 75, 30}, 11);
        const float volume = master ? displayProject().masterVolume
                                    : displayProject().channels[static_cast<std::size_t>(i - 1)].volume;
        const float peak = master ? std::max(audio_.outputPeakLeft.load(), audio_.outputPeakRight.load())
                                  : audio_.renderer.channelPeak(static_cast<std::size_t>(i - 1));
        const int faderTop = top + 100, height = std::max(20, bottom - faderTop);
        g.setColour(raised);
        g.fillRect(x + 38, faderTop, 3, height);
        const int handle = faderTop + static_cast<int>((1 - volume) * height);
        box(g, {x + 27, handle - 7, 25, 15}, juce::Colour(0xffaab7ba));
        g.setColour(grid);
        g.fillRect(x + 60, faderTop, 9, height);
        g.setColour(peak > 0.9f ? juce::Colour(0xffdd7777) : accent);
        g.fillRect(x + 60, bottom - static_cast<int>(std::min(1.0f, peak) * height), 9,
                   static_cast<int>(std::min(1.0f, peak) * height));
        text(g, juce::String(volume > 0 ? 20 * std::log10(volume) : -90, 1) + " dB",
             {x + 4, bottom + 5, 76, 23}, 10, muted, juce::Justification::centred);
        if (!master)
        {
            const auto &c = displayProject().channels[static_cast<std::size_t>(i - 1)];
            box(g, {x + 8, top + 56, 29, 22}, c.muted ? juce::Colour(0xffbd7774) : raised);
            box(g, {x + 45, top + 56, 29, 22}, c.soloed ? accent.darker(0.25f) : raised);
            text(g, "M", {x + 8, top + 56, 29, 22}, 10, ink, juce::Justification::centred);
            text(g, "S", {x + 45, top + 56, 29, 22}, 10, ink, juce::Justification::centred);
        }
    }
    const int x = editorBounds_.getX() + 16 + count * 92;
    const int width = editorBounds_.getRight() - x - 16;
    if (width > 150)
    {
        box(g, {x, top, width, bottom - top + 32}, panel.darker(0.2f));
        text(g, "CHANNEL PARAMETERS", {x + 16, top + 10, width - 32, 24}, 11, muted);
        if (!displayProject().channels.empty())
        {
            const auto &c = displayProject().channels[static_cast<std::size_t>(selectedChannel_)];
            text(g, c.name, {x + 16, top + 48, width - 32, 30}, 18, juce::Colour(c.color));
            const char *labels[] = {"LOW-PASS FILTER", "DELAY SEND",  "PAN",
                                    "REVERB",          "COMPRESSION", "SATURATION"};
            const float values[] = {c.cutoff, c.delay, (c.pan + 1) * 0.5f, c.reverb, c.compression, c.drive};
            for (int k = 0; k < 6; ++k)
            {
                int y = top + 104 + k * 44;
                text(g, labels[k], {x + 16, y, width - 32, 20}, 10, muted);
                box(g, {x + 16, y + 28, width - 32, 7}, grid);
                g.setColour(juce::Colour(c.color));
                g.fillRect(x + 16, y + 28, static_cast<int>((width - 32) * values[k]), 7);
            }
        }
    }
}
void Workspace::drawRack(juce::Graphics &g)
{
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRect(rackBounds_.translated(5, 6));
    box(g, rackBounds_, panel);
    box(g, rackBounds_.withHeight(33), raised);
    text(g, "CHANNEL RACK", rackBounds_.withHeight(33).reduced(13, 0), 12);
    text(g, "16 STEPS   /   " + (activePattern() ? juce::String(activePattern()->name) : ""),
         rackBounds_.withHeight(33).reduced(13, 0), 11, muted, juce::Justification::centredRight);
    const int startX = rackBounds_.getX() + 225;
    const int stepWidth = std::max(12, (rackBounds_.getWidth() - 242) / 16);
    const auto *pat = activePattern();
    for (std::size_t i = 0; i < displayProject().channels.size() && i < 6; ++i)
    {
        const auto &c = displayProject().channels[i];
        const int y = rackBounds_.getY() + 43 + static_cast<int>(i) * 31;
        const auto color = juce::Colour(c.color);
        g.setColour(c.muted ? muted : accent);
        g.fillEllipse(static_cast<float>(rackBounds_.getX() + 13), static_cast<float>(y + 9), 8, 8);
        box(g, {rackBounds_.getX() + 32, y + 2, 178, 25},
            static_cast<int>(i) == selectedChannel_ ? color.darker(0.55f) : raised);
        text(g, c.name, {rackBounds_.getX() + 41, y + 2, 155, 25}, 11);
        for (int step = 0; step < 16; ++step)
        {
            const bool active = pat && std::any_of(pat->notes.begin(), pat->notes.end(),
                                                   [&](const auto &n)
                                                   {
                                                       return n.channel == static_cast<int>(i) &&
                                                              std::abs(n.startBeat - step * 0.25) < 0.01;
                                                   });
            auto r = juce::Rectangle<int>(startX + step * stepWidth, y + 3, stepWidth - 4, 24);
            box(g, r, active ? color : (step / 4) % 2 == 0 ? raised : raised.brighter(0.12f));
            if (active)
            {
                g.setColour(color.brighter(0.4f));
                g.fillRect(r.reduced(3).withHeight(3));
            }
            if (audio_.renderer.playing() &&
                static_cast<int>(std::fmod(audio_.renderer.positionBeat(), 4) * 4) == step)
            {
                g.setColour(accent);
                g.drawRect(r.expanded(1), 1);
            }
        }
    }
}
void Workspace::mouseDown(const juce::MouseEvent &e)
{
    grabKeyboardFocus();
    const auto pos = e.getPosition();
    draggedNote_ = dragTrack_ = dragClip_ = dragFader_ = -1;
    if (showRack_ && rackBounds_.contains(pos))
    {
        int channel = (pos.y - rackBounds_.getY() - 43) / 31;
        if (pos.y < rackBounds_.getY() + 43 || channel < 0 ||
            channel >= static_cast<int>(project_.channels.size()) || channel >= 6)
            return;
        selectedChannel_ = channel;
        audio_.renderer.setMidiChannel(channel);
        channels_.setSelectedId(channel + 1, juce::dontSendNotification);
        if (pos.x < rackBounds_.getX() + 30)
        {
            edit("Mute channel",
                 [=](Project &p)
                 {
                     p.channels[static_cast<std::size_t>(channel)].muted =
                         !p.channels[static_cast<std::size_t>(channel)].muted;
                 });
            return;
        }
        const int startX = rackBounds_.getX() + 225, sw = std::max(12, (rackBounds_.getWidth() - 242) / 16),
                  step = (pos.x - startX) / sw;
        if (pos.x >= startX && step >= 0 && step < 16)
        {
            const auto id = selected_;
            edit("Toggle step",
                 [=](Project &p)
                 {
                     auto *pat = p.findPattern(id);
                     if (!pat)
                         return;
                     auto it = std::find_if(
                         pat->notes.begin(), pat->notes.end(), [=](auto &n)
                         { return n.channel == channel && std::abs(n.startBeat - step * 0.25) < 0.01; });
                     if (it != pat->notes.end())
                         pat->notes.erase(it);
                     else
                         pat->notes.push_back({channel,
                                               channel == 0   ? 36
                                               : channel == 1 ? 38
                                               : channel == 2 ? 42
                                                              : 60,
                                               step * 0.25, 0.2, 0.8f});
                 });
        }
        repaint();
        return;
    }
    if (view_ == View::Mixer && editorBounds_.contains(pos))
    {
        const int index = (pos.x - editorBounds_.getX() - 16) / 92;
        const int top = editorBounds_.getY() + 55;
        const int count = static_cast<int>(project_.channels.size()) + 1;
        if (index >= 0 && index < count && pos.x >= editorBounds_.getX() + 16)
        {
            if (index > 0 && pos.y >= top + 56 && pos.y < top + 78)
            {
                const int relative = pos.x - (editorBounds_.getX() + 16 + index * 92);
                edit("Mute / solo",
                     [=](Project &p)
                     {
                         auto &c = p.channels[static_cast<std::size_t>(index - 1)];
                         if (relative < 42)
                             c.muted = !c.muted;
                         else
                             c.soloed = !c.soloed;
                     });
                return;
            }
            if (index > 0)
            {
                selectedChannel_ = index - 1;
                audio_.renderer.setMidiChannel(selectedChannel_);
                channels_.setSelectedId(index, juce::dontSendNotification);
            }
            if (pos.y >= top + 100)
            {
                dragFader_ = index;
                beforeDrag_ = project_;
                preview_ = project_;
                mouseDrag(e);
            }
            return;
        }
        const int x = editorBounds_.getX() + 16 + count * 92, width = editorBounds_.getRight() - x - 16;
        const int k = (pos.y - top - 104) / 44;
        if (width > 150 && k >= 0 && k < 6 && pos.y >= top + 104 && !project_.channels.empty())
        {
            const float value = std::clamp(static_cast<float>(pos.x - x - 16) / (width - 32), 0.0f, 1.0f);
            const int c = selectedChannel_;
            edit("Set channel parameter",
                 [=](Project &p)
                 {
                     auto &ch = p.channels[static_cast<std::size_t>(c)];
                     if (k == 0)
                         ch.cutoff = value;
                     else if (k == 1)
                         ch.delay = value;
                     else if (k == 2)
                         ch.pan = value * 2 - 1;
                     else if (k == 3)
                         ch.reverb = value;
                     else if (k == 4)
                         ch.compression = value;
                     else
                         ch.drive = value;
                 });
        }
        return;
    }
    const auto area = timeline();
    if (view_ == View::Piano && pos.y >= area.getBottom() && pos.y < area.getBottom() + 72 &&
        pos.x >= area.getX())
    {
        const double beat = (pos.x - area.getX()) / (beatWidth_ * 2);
        const float velocity = std::clamp((area.getBottom() + 62.0f - pos.y) / 48.0f, 0.05f, 1.0f);
        const auto id = selected_;
        const int channel = selectedChannel_;
        edit("Set note velocity",
             [=](Project &p)
             {
                 auto *pat = p.findPattern(id);
                 if (!pat)
                     return;
                 auto it = std::min_element(
                     pat->notes.begin(), pat->notes.end(),
                     [=](const auto &a, const auto &b)
                     {
                         const double ad = a.channel == channel ? std::abs(a.startBeat - beat) : 1000000,
                                      bd = b.channel == channel ? std::abs(b.startBeat - beat) : 1000000;
                         return ad < bd;
                     });
                 if (it != pat->notes.end() && it->channel == channel &&
                     std::abs(it->startBeat - beat) < 0.25)
                     it->velocity = velocity;
             });
        return;
    }
    if (!area.contains(pos))
        return;
    if (view_ == View::Piano)
    {
        auto *pat = activePattern();
        if (!pat)
            return;
        const double beat =
            std::max(0.0, static_cast<double>(std::floor((pos.x - area.getX()) / (beatWidth_ * 2) * 4) / 4));
        const int pitch = pianoTop_ - (pos.y - area.getY()) / 18;
        for (int i = static_cast<int>(pat->notes.size()) - 1; i >= 0; --i)
        {
            const auto &n = pat->notes[static_cast<std::size_t>(i)];
            if (n.channel != selectedChannel_ || n.pitch != pitch)
                continue;
            if (beat >= n.startBeat && beat < n.startBeat + n.lengthBeats)
            {
                if (e.mods.isRightButtonDown())
                {
                    const auto id = selected_;
                    edit("Delete note",
                         [=](Project &p)
                         {
                             auto *a = p.findPattern(id);
                             a->notes.erase(a->notes.begin() + i);
                         });
                }
                else
                {
                    draggedNote_ = i;
                    beforeDrag_ = project_;
                    preview_ = project_;
                    dragBeat_ = n.startBeat;
                    dragPitch_ = n.pitch;
                    const int edge =
                        area.getX() + static_cast<int>((n.startBeat + n.lengthBeats) * beatWidth_ * 2);
                    resizingNote_ = pos.x > edge - 8;
                }
                return;
            }
        }
        if (!e.mods.isRightButtonDown() && pitch >= 0 && pitch <= 127 && beat < pat->lengthBeats)
        {
            const auto id = selected_;
            const int ch = selectedChannel_;
            edit("Add note",
                 [=](Project &p) { p.findPattern(id)->notes.push_back({ch, pitch, beat, 0.5, 0.8f}); });
        }
        return;
    }
    if (view_ == View::Playlist)
    {
        const int track = (pos.y - area.getY()) / 72;
        if (track >= static_cast<int>(project_.tracks.size()))
        {
            const int lane = track - static_cast<int>(project_.tracks.size());
            if (lane >= static_cast<int>(project_.automation.size()))
                return;
            const double beat =
                std::max(0.0, static_cast<double>(std::round((pos.x - area.getX()) / beatWidth_ * 4) / 4));
            const float value = std::clamp((61.0f - (pos.y - area.getY() - track * 72)) / 48.0f, 0.0f, 1.0f);
            const bool erase = e.mods.isRightButtonDown();
            edit("Edit automation point",
                 [=](Project &p)
                 {
                     auto &points = p.automation[static_cast<std::size_t>(lane)].points;
                     auto nearest =
                         std::min_element(points.begin(), points.end(), [=](const auto &a, const auto &b)
                                          { return std::abs(a.beat - beat) < std::abs(b.beat - beat); });
                     if (nearest != points.end() && std::abs(nearest->beat - beat) < 0.5)
                     {
                         if (erase && points.size() > 1)
                             points.erase(nearest);
                         else if (!erase)
                             nearest->value = value;
                     }
                     else if (!erase)
                     {
                         points.push_back({beat, value});
                         std::sort(points.begin(), points.end(),
                                   [](auto a, auto b) { return a.beat < b.beat; });
                     }
                 });
            return;
        }
        const double beat = (pos.x - area.getX()) / beatWidth_;
        auto &clips = project_.tracks[static_cast<std::size_t>(track)].clips;
        for (int i = static_cast<int>(clips.size()) - 1; i >= 0; --i)
        {
            const auto &c = clips[static_cast<std::size_t>(i)];
            if (beat >= c.startBeat && beat < c.startBeat + c.lengthBeats)
            {
                if (e.mods.isRightButtonDown())
                    edit("Delete clip",
                         [=](Project &p)
                         {
                             auto &cs = p.tracks[static_cast<std::size_t>(track)].clips;
                             cs.erase(cs.begin() + i);
                         });
                else if (e.mods.isShiftDown())
                {
                    const double at = std::floor(beat * 4) / 4;
                    edit("Slice clip",
                         [=](Project &p)
                         {
                             auto &clips = p.tracks[static_cast<std::size_t>(track)].clips;
                             const auto original = clips[static_cast<std::size_t>(i)];
                             if (at <= original.startBeat || at >= original.startBeat + original.lengthBeats)
                                 return;
                             auto right = original;
                             right.id = ClipID{p.nextId()};
                             right.startBeat = at;
                             right.lengthBeats = original.startBeat + original.lengthBeats - at;
                             clips[static_cast<std::size_t>(i)].lengthBeats = at - original.startBeat;
                             clips.push_back(right);
                         });
                }
                else if (e.mods.isCtrlDown() || e.mods.isCommandDown())
                {
                    edit("Duplicate clip",
                         [=](Project &p)
                         {
                             auto c =
                                 p.tracks[static_cast<std::size_t>(track)].clips[static_cast<std::size_t>(i)];
                             c.id = ClipID{p.nextId()};
                             c.startBeat += c.lengthBeats;
                             p.tracks[static_cast<std::size_t>(track)].clips.push_back(c);
                         });
                }
                else
                {
                    beforeDrag_ = project_;
                    preview_ = project_;
                    dragTrack_ = track;
                    dragClip_ = i;
                    dragBeat_ = c.startBeat;
                    resizingClip_ = pos.x > area.getX() +
                                                static_cast<int>((c.startBeat + c.lengthBeats) * beatWidth_) -
                                                8;
                }
                return;
            }
        }
        if (!e.mods.isRightButtonDown())
        {
            const auto pattern = selected_;
            edit("Add pattern clip",
                 [=](Project &p)
                 {
                     const auto *pat = p.findPattern(pattern);
                     p.tracks[static_cast<std::size_t>(track)].clips.push_back(
                         {ClipID{p.nextId()}, pattern, std::max(0.0, std::floor(beat)), pat->lengthBeats, 0,
                          false});
                 });
        }
    }
}
void Workspace::mouseDrag(const juce::MouseEvent &e)
{
    if (dragFader_ >= 0)
    {
        const int top = editorBounds_.getY() + 155, bottom = editorBounds_.getBottom() - 44;
        const float volume =
            std::clamp(1.0f - static_cast<float>(e.y - top) / std::max(1, bottom - top), 0.0f, 1.0f);
        if (dragFader_ == 0)
            preview_->masterVolume = volume;
        else
            preview_->channels[static_cast<std::size_t>(dragFader_ - 1)].volume = volume;
        sync();
        return;
    }
    if (draggedNote_ >= 0)
    {
        auto *p = preview_->findPattern(selected_);
        if (!p)
            return;
        auto &n = p->notes[static_cast<std::size_t>(draggedNote_)];
        const double delta = std::round(e.getDistanceFromDragStartX() / (beatWidth_ * 2) * 4) / 4;
        if (resizingNote_)
            n.lengthBeats = std::clamp(beforeDrag_.findPattern(selected_)
                                               ->notes[static_cast<std::size_t>(draggedNote_)]
                                               .lengthBeats +
                                           delta,
                                       0.25, 1024.0);
        else
        {
            n.startBeat = std::clamp(dragBeat_ + delta, 0.0, p->lengthBeats - 0.25);
            n.pitch = std::clamp(dragPitch_ - e.getDistanceFromDragStartY() / 18, 0, 127);
        }
        repaint();
        return;
    }
    if (dragClip_ >= 0)
    {
        auto &clip =
            preview_->tracks[static_cast<std::size_t>(dragTrack_)].clips[static_cast<std::size_t>(dragClip_)];
        const double delta = std::round(e.getDistanceFromDragStartX() / beatWidth_ * 4) / 4;
        if (resizingClip_)
            clip.lengthBeats = std::clamp(beforeDrag_.tracks[static_cast<std::size_t>(dragTrack_)]
                                                  .clips[static_cast<std::size_t>(dragClip_)]
                                                  .lengthBeats +
                                              delta,
                                          0.25, 4096.0);
        else
            clip.startBeat = std::max(0.0, dragBeat_ + delta);
        repaint();
    }
}
void Workspace::mouseUp(const juce::MouseEvent &)
{
    if (draggedNote_ >= 0 || dragClip_ >= 0 || dragFader_ >= 0)
    {
        const auto after = *preview_;
        preview_.reset();
        edit("Edit selection", [&](Project &p) { p = after; });
        draggedNote_ = dragClip_ = dragTrack_ = dragFader_ = -1;
    }
}
void Workspace::mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &w)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        beatWidth_ = std::clamp(beatWidth_ * (w.deltaY > 0 ? 1.12f : 0.89f), 8.0f, 120.0f);
    else if (view_ == View::Piano)
        pianoTop_ = std::clamp(pianoTop_ + (w.deltaY > 0 ? 2 : -2), 24, 127);
    repaint();
}
bool Workspace::keyPressed(const juce::KeyPress &key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        transportToggle();
        return true;
    }
    if (key == juce::KeyPress::F5Key)
    {
        selectView(View::Playlist);
        return true;
    }
    if (key == juce::KeyPress::F6Key)
    {
        showRack_ = !showRack_;
        resized();
        repaint();
        return true;
    }
    if (key == juce::KeyPress::F7Key)
    {
        selectView(View::Piano);
        return true;
    }
    if (key == juce::KeyPress::F9Key)
    {
        selectView(View::Mixer);
        return true;
    }
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
    {
        if (key.getModifiers().isShiftDown() && key.getKeyCode() == 'N')
        {
            edit("Add synth channel",
                 [](Project &p)
                 {
                     if (p.channels.size() >= maxChannels)
                         return;
                     Channel c;
                     c.id = ChannelID{p.nextId()};
                     c.name = "Forge Synth " + std::to_string(p.channels.size() + 1);
                     p.channels.push_back(c);
                 });
            sync(true);
            return true;
        }
        if (key.getModifiers().isShiftDown() && key.getKeyCode() == 'P')
        {
            edit("Add pattern",
                 [](Project &p)
                 {
                     Pattern pat;
                     pat.id = PatternID{p.nextId()};
                     pat.name = "Pattern " + std::to_string(p.patterns.size() + 1);
                     p.patterns.push_back(pat);
                 });
            selected_ = project_.patterns.back().id;
            sync(true);
            return true;
        }
        if (key.getModifiers().isShiftDown() && key.getKeyCode() == 'T')
        {
            edit("Add track",
                 [](Project &p)
                 {
                     Track t;
                     t.id = TrackID{p.nextId()};
                     t.name = "Track " + std::to_string(p.tracks.size() + 1);
                     p.tracks.push_back(t);
                 });
            sync();
            return true;
        }
        if (key.getKeyCode() == 'Q')
        {
            const auto id = selected_;
            const int channel = selectedChannel_;
            edit("Quantize notes",
                 [=](Project &p)
                 {
                     auto *pat = p.findPattern(id);
                     if (!pat)
                         return;
                     for (auto &n : pat->notes)
                         if (n.channel == channel)
                             n.startBeat =
                                 std::clamp(std::round(n.startBeat * 4) / 4, 0.0, pat->lengthBeats - 0.25);
                 });
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
        {
            const int semitones = (key.getKeyCode() == juce::KeyPress::upKey ? 1 : -1) *
                                  (key.getModifiers().isShiftDown() ? 12 : 1);
            const auto id = selected_;
            const int channel = selectedChannel_;
            edit("Transpose notes",
                 [=](Project &p)
                 {
                     if (auto *pat = p.findPattern(id))
                         for (auto &n : pat->notes)
                             if (n.channel == channel)
                                 n.pitch = std::clamp(n.pitch + semitones, 0, 127);
                 });
            return true;
        }
        if (key.getKeyCode() == 'S')
        {
            chooseSave();
            return true;
        }
        if (key.getKeyCode() == 'O')
        {
            chooseOpen();
            return true;
        }
        if (key.getKeyCode() == 'Z')
        {
            if (key.getModifiers().isShiftDown())
                redoButton_.triggerClick();
            else
                undoButton_.triggerClick();
            return true;
        }
        if (key.getKeyCode() == 'Y')
        {
            redoButton_.triggerClick();
            return true;
        }
    }
    return false;
}
void Workspace::restoreSamplesAsync()
{
    closePluginEditor();
    pluginsLoading_ = true;
    pluginsButton_.setEnabled(false);
    const auto project = project_;
    const juce::Component::SafePointer<Workspace> self(this);
    workers_.addJob(
        [self, project]
        {
            if (!self)
                return;
            auto r = self->audio_.restoreSamples(project);
            if (r)
                r = self->audio_.restorePlugins(project.masterInserts);
            juce::MessageManager::callAsync(
                [self]
                {
                    if (self)
                    {
                        self->pluginsLoading_ = false;
                        self->pluginsButton_.setEnabled(true);
                    }
                });
            if (!r)
                juce::MessageManager::callAsync(
                    [self, r]
                    {
                        if (self)
                            self->message_ = r.error;
                    });
        });
}
void Workspace::toggleMidiRecording()
{
    if (audio_.renderer.recording())
    {
        audio_.renderer.setRecording(false);
        CapturedMidi event;
        while (audio_.renderer.capture(event))
            midiRecorder_.receive(event);
        const auto notes = midiRecorder_.finish(audio_.renderer.positionBeat());
        const auto id = recordPattern_;
        edit("Record MIDI",
             [&](Project &p)
             {
                 if (auto *pat = p.findPattern(id))
                     pat->notes.insert(pat->notes.end(), notes.begin(), notes.end());
             });
        patterns_.setEnabled(true);
        channels_.setEnabled(true);
        midiRecord_.setToggleState(false, juce::dontSendNotification);
    }
    else
    {
        auto *pat = activePattern();
        if (!pat)
            return;
        recordPattern_ = pat->id;
        midiRecorder_.begin(pat->lengthBeats, selectedChannel_);
        CapturedMidi event;
        while (audio_.renderer.capture(event))
        {
        }
        audio_.renderer.setRecording(true);
        audio_.renderer.setPlaying(true);
        midiRecord_.setToggleState(true, juce::dontSendNotification);
        patterns_.setEnabled(false);
        channels_.setEnabled(false);
        message_ = "Recording MIDI into " + juce::String(pat->name);
    }
}
void Workspace::toggleAudioRecording()
{
    const juce::Component::SafePointer<Workspace> self(this);
    if (audio_.recordingAudio())
    {
        workers_.addJob(
            [self]
            {
                if (!self)
                    return;
                self->audio_.stopRecording();
                juce::MessageManager::callAsync(
                    [self]
                    {
                        if (self)
                        {
                            self->audioRecord_.setToggleState(false, juce::dontSendNotification);
                            self->message_ =
                                juce::String::fromUTF8("Input recording saved · Dropped blocks: ") +
                                juce::String(self->audio_.recordDrops.load());
                        }
                    });
            });
        return;
    }
    chooser_ = std::make_unique<juce::FileChooser>(
        "Record audio input to stereo WAV",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Take.wav"), "*.wav");
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
                          [self](const juce::FileChooser &c)
                          {
                              if (!self || c.getResult() == juce::File{})
                                  return;
                              const auto file = c.getResult().withFileExtension("wav");
                              self->workers_.addJob(
                                  [self, file]
                                  {
                                      if (!self)
                                          return;
                                      const auto result = self->audio_.startRecording(file);
                                      juce::MessageManager::callAsync(
                                          [self, result]
                                          {
                                              if (!self)
                                                  return;
                                              self->audioRecord_.setToggleState(static_cast<bool>(result),
                                                                                juce::dontSendNotification);
                                              self->message_ =
                                                  result ? juce::String::fromUTF8("Recording audio input…")
                                                         : juce::String(result.error);
                                          });
                                  });
                          });
}
bool Workspace::isInterestedInFileDrag(const juce::StringArray &files)
{
    for (const auto &file : files)
        if (juce::File(file).hasFileExtension("wav;aiff;aif;flac;ogg;mp3"))
            return true;
    return false;
}
void Workspace::filesDropped(const juce::StringArray &files, int, int y)
{
    int channel = selectedChannel_;
    if (showRack_ && y >= rackBounds_.getY() + 43 && y < rackBounds_.getBottom())
        channel =
            std::clamp((y - rackBounds_.getY() - 43) / 31, 0, static_cast<int>(project_.channels.size()) - 1);
    if (!files.isEmpty() && !project_.channels.empty())
        importSample(juce::File(files[0]), channel);
}
void Workspace::importSample(const juce::File &file, int channel)
{
    const juce::Component::SafePointer<Workspace> self(this);
    message_ = juce::String::fromUTF8("Decoding sample…");
    workers_.addJob(
        [self, file, channel]
        {
            if (!self)
                return;
            const auto result = self->audio_.loadSample(static_cast<std::size_t>(channel), file);
            juce::MessageManager::callAsync(
                [self, file, channel, result]
                {
                    if (!self)
                        return;
                    if (!result)
                    {
                        self->message_ = result.error;
                        return;
                    }
                    self->edit("Import sample",
                               [=](Project &p)
                               {
                                   auto &c = p.channels[static_cast<std::size_t>(channel)];
                                   c.samplePath = file.getFullPathName().toStdString();
                                   c.name = file.getFileNameWithoutExtension().toStdString();
                                   c.instrument = InstrumentType::Sampler;
                               });
                    self->sync(true);
                });
        });
}
void Workspace::saveTo(const juce::File &file, bool autosave)
{
    const auto project = project_;
    const juce::Component::SafePointer<Workspace> self(this);
    workers_.addJob(
        [project, file, autosave, self]
        {
            file.getParentDirectory().createDirectory();
            auto serialised = project;
            auto result =
                self ? self->audio_.capturePlugins(serialised) : Result::failure("Workspace closed");
            if (result)
                result = saveProject(serialised, file.getFullPathName().toStdString());
            juce::MessageManager::callAsync(
                [self, result, autosave, file, project]
                {
                    if (!self)
                        return;
                    if (!result)
                        self->message_ = result.error;
                    else
                    {
                        self->message_ = autosave ? "Recovery snapshot saved" : "Saved " + file.getFileName();
                        if (!autosave)
                        {
                            self->currentFile_ = file;
                            if (self->project_ == project)
                                self->dirty_ = false;
                        }
                    }
                    self->repaint();
                });
        });
}
void Workspace::chooseSave()
{
    if (currentFile_.existsAsFile())
    {
        saveTo(currentFile_);
        return;
    }
    chooser_ = std::make_unique<juce::FileChooser>(
        "Save LS Factory project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Session.odaw"),
        "*.odaw");
    const juce::Component::SafePointer<Workspace> self(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
                          [self](const juce::FileChooser &c)
                          {
                              if (self && c.getResult() != juce::File{})
                                  self->saveTo(c.getResult().withFileExtension("odaw"));
                          });
}
void Workspace::openProject(const juce::File &file)
{
    if (pluginsLoading_)
    {
        message_ = "Wait for plugin loading to finish";
        return;
    }
    const juce::Component::SafePointer<Workspace> self(this);
    workers_.addJob(
        [self, file]
        {
            Project loaded;
            const auto result = loadProject(file.getFullPathName().toStdString(), loaded);
            juce::MessageManager::callAsync(
                [self, file, result, loaded]
                {
                    if (!self)
                        return;
                    if (!result)
                    {
                        self->message_ = result.error;
                        return;
                    }
                    self->audio_.renderer.setPlaying(false);
                    self->audio_.renderer.rewind();
                    self->closePluginEditor();
                    self->pluginsLoading_ = true;
                    self->pluginsButton_.setEnabled(false);
                    self->project_ = loaded;
                    self->undo_.clear();
                    self->currentFile_ = file;
                    self->dirty_ = false;
                    self->sync(true);
                    self->message_ = "Loaded " + file.getFileName();
                    self->workers_.addJob(
                        [self, loaded]
                        {
                            if (!self)
                                return;
                            auto restored = self->audio_.restoreSamples(loaded);
                            if (restored)
                                restored = self->audio_.restorePlugins(loaded.masterInserts);
                            juce::MessageManager::callAsync(
                                [self, restored]
                                {
                                    if (!self)
                                        return;
                                    self->pluginsLoading_ = false;
                                    self->pluginsButton_.setEnabled(true);
                                    if (!restored)
                                        self->message_ = restored.error;
                                });
                        });
                });
        });
}
void Workspace::chooseOpen()
{
    chooser_ = std::make_unique<juce::FileChooser>(
        "Open project or recovery snapshot",
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("LSFactory"),
        "*.odaw");
    const juce::Component::SafePointer<Workspace> self(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [self](const juce::FileChooser &chooser)
                          {
                              if (self && chooser.getResult() != juce::File{})
                                  self->openProject(chooser.getResult());
                          });
}
void Workspace::chooseExport()
{
    chooser_ = std::make_unique<juce::FileChooser>(
        "Export arrangement as 24-bit stereo WAV",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Afterglow.wav"),
        "*.wav");
    const juce::Component::SafePointer<Workspace> self(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
                          [self](const juce::FileChooser &c)
                          {
                              if (!self || c.getResult() == juce::File{})
                                  return;
                              const auto p = self->project_;
                              const auto file = c.getResult().withFileExtension("wav");
                              self->message_ = juce::String::fromUTF8("Rendering WAV…");
                              self->workers_.addJob(
                                  [self, p, file]
                                  {
                                      AudioEngine offline;
                                      auto serialised = p;
                                      auto r = self ? self->audio_.capturePlugins(serialised)
                                                    : Result::failure("Workspace closed");
                                      if (r)
                                          r = offline.exportWave(serialised, file);
                                      juce::MessageManager::callAsync(
                                          [self, r, file]
                                          {
                                              if (self)
                                                  self->message_ = r ? "Exported " + file.getFileName()
                                                                     : juce::String(r.error);
                                          });
                                  });
                          });
}
namespace
{
class PluginEditorWindow final : public juce::DocumentWindow
{
  public:
    explicit PluginEditorWindow(juce::AudioPluginInstance &processor)
        : DocumentWindow(processor.getName(), juce::Colour(0xff303a40), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        auto *editor = processor.createEditorAndMakeActive();
        if (!editor)
            editor = new juce::GenericAudioProcessorEditor(processor);
        setContentOwned(editor, true);
        setResizable(true, false);
        centreWithSize(editor->getWidth(), editor->getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override
    {
        setVisible(false);
    }
};
} // namespace
void Workspace::closePluginEditor()
{
    if (listeningProcessor_)
        listeningProcessor_->removeListener(this);
    listeningProcessor_ = nullptr;
    pluginWindow_.reset();
}
void Workspace::showPlugins()
{
    if (pluginsLoading_)
        return;
    juce::PopupMenu menu;
    menu.addItem(1, "Add stereo VST3 master effect", project_.masterInserts.size() < 8);
    for (std::size_t i = 0; i < project_.masterInserts.size(); ++i)
    {
        const auto &slot = project_.masterInserts[i];
        juce::PopupMenu entry;
        entry.addItem(100 + static_cast<int>(i), "Open editor");
        entry.addItem(200 + static_cast<int>(i), "Bypass", true, slot.bypassed);
        entry.addItem(300 + static_cast<int>(i), "Remove");
        menu.addSubMenu(slot.name, entry);
    }
    const juce::Component::SafePointer<Workspace> self(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options{}.withTargetComponent(pluginsButton_),
        [self](int result)
        {
            if (!self || self->pluginsLoading_)
                return;
            if (result == 1)
            {
                self->choosePlugin();
                return;
            }
            if (result >= 100 && result < 108)
            {
                auto *processor = self->audio_.plugins.processor(static_cast<std::size_t>(result - 100));
                if (processor)
                {
                    self->closePluginEditor();
                    self->listeningProcessor_ = processor;
                    processor->addListener(self.getComponent());
                    self->pluginWindow_ = std::make_unique<PluginEditorWindow>(*processor);
                }
                return;
            }
            auto slots = self->project_.masterInserts;
            if (result >= 200 && result < 208 && static_cast<std::size_t>(result - 200) < slots.size())
            {
                slots[static_cast<std::size_t>(result - 200)].bypassed =
                    !slots[static_cast<std::size_t>(result - 200)].bypassed;
                self->applyPluginChain(slots, "Toggle plugin bypass");
            }
            else if (result >= 300 && result < 308 && static_cast<std::size_t>(result - 300) < slots.size())
            {
                slots.erase(slots.begin() + result - 300);
                self->applyPluginChain(slots, "Remove master insert");
            }
        });
}
void Workspace::applyPluginChain(const std::vector<PluginSlot> &slots, std::string description)
{
    closePluginEditor();
    pluginsLoading_ = true;
    pluginsButton_.setEnabled(false);
    const juce::Component::SafePointer<Workspace> self(this);
    // Capture edited states first; reuse surviving slots so bypass/removal doesn't discard parameter edits.
    auto model = project_;
    workers_.addJob(
        [self, slots, description, model]() mutable
        {
            if (!self)
                return;
            auto result = self->audio_.capturePlugins(model);
            auto proposed = slots;
            if (result)
            {
                for (auto &slot : proposed)
                    for (const auto &existing : model.masterInserts)
                        if (slot.id == existing.id)
                        {
                            slot.state = existing.state;
                            slot.normalizedParameters = existing.normalizedParameters;
                        }
                result = self->audio_.restorePlugins(proposed);
            }
            juce::MessageManager::callAsync(
                [self, result, proposed, description]
                {
                    if (!self)
                        return;
                    self->pluginsLoading_ = false;
                    self->pluginsButton_.setEnabled(true);
                    if (!result)
                    {
                        self->message_ = result.error;
                        return;
                    }
                    self->edit(description, [&](Project &p) { p.masterInserts = proposed; });
                });
        });
}
void Workspace::choosePlugin()
{
    chooser_ =
        std::make_unique<juce::FileChooser>("Load a stereo VST3 effect bundle", juce::File{}, "*.vst3");
    const juce::Component::SafePointer<Workspace> self(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::canSelectDirectories,
                          [self](const juce::FileChooser &chooser)
                          {
                              if (!self || chooser.getResult() == juce::File{})
                                  return;
                              const auto file = chooser.getResult();
                              self->pluginsLoading_ = true;
                              self->pluginsButton_.setEnabled(false);
                              self->message_ = "Scanning plugin in an isolated process...";
                              self->workers_.addJob(
                                  [self, file]
                                  {
                                      std::vector<PluginSlot> found;
                                      const auto result = PluginManager::scanIsolated(file, found);
                                      juce::MessageManager::callAsync(
                                          [self, result, found]
                                          {
                                              if (!self)
                                                  return;
                                              self->pluginsLoading_ = false;
                                              self->pluginsButton_.setEnabled(true);
                                              if (!result)
                                              {
                                                  self->message_ = result.error;
                                                  return;
                                              }
                                              auto slots = self->project_.masterInserts;
                                              auto slot = found.front();
                                              slot.id = PluginInstanceID{self->project_.nextId()};
                                              slots.push_back(std::move(slot));
                                              self->applyPluginChain(slots, "Add master insert");
                                          });
                                  });
                          });
}
void Workspace::showSettings()
{
    juce::DialogWindow::LaunchOptions options;
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(audio_.devices, 0, 2, 0, 2, false,
                                                                         false, true, false);
    selector->setSize(520, 420);
    options.content.setOwned(selector.release());
    options.dialogTitle = juce::String::fromUTF8("LS Factory · Audio devices");
    options.dialogBackgroundColour = panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
bool Workspace::smokeTest(const juce::File &directory)
{
    const auto original = project_;
    if (!exportJson(project_, directory.getChildFile("demo.json").getFullPathName().toStdString()))
        return false;
    edit("Smoke add note",
         [this](Project &p) { p.findPattern(selected_)->notes.push_back({0, 36, 0.25, 0.25, 0.5f}); });
    if (project_ == original)
        return false;
    if (!undo_.undo(project_) || project_ != original)
        return false;
    if (!undo_.redo(project_) || project_ == original)
        return false;
    const auto file = directory.getChildFile("smoke.odaw");
    if (!saveProject(project_, file.getFullPathName().toStdString()))
        return false;
    Project loaded;
    if (!loadProject(file.getFullPathName().toStdString(), loaded) || loaded != project_)
        return false;
    project_ = original;
    sync(true);
    for (auto view : {View::Playlist, View::Piano, View::Mixer})
    {
        if (view == View::Piano)
        {
            selected_ = project_.patterns.back().id;
            selectedChannel_ = 4;
            showRack_ = false;
            sync(true);
        }
        selectView(view);
        auto image = createComponentSnapshot(getLocalBounds());
        juce::FileOutputStream out(directory.getChildFile(view == View::Playlist ? "playlist.png"
                                                          : view == View::Piano  ? "piano.png"
                                                                                 : "mixer.png"));
        out.setPosition(0);
        out.truncate();
        if (!juce::PNGImageFormat().writeImageToStream(image, out))
            return false;
    }
    AudioEngine recorder;
    const auto recording = directory.getChildFile("input-test.wav");
    if (!recorder.startRecording(recording, true))
        return false;
    std::array<float, 512> input{};
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<float>(std::sin(i * 0.05)) * 0.25f;
    const float *pointers[] = {input.data()};
    for (int block = 0; block < 10; ++block)
        recorder.captureInput(pointers, 1, 512);
    recorder.stopRecording();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(recording));
    if (!reader || reader->lengthInSamples != 5120)
        return false;
    return static_cast<bool>(audio_.exportWave(project_, directory.getChildFile("demo.wav")));
}
MainWindow::MainWindow(bool device) : DocumentWindow("LS Factory", panel, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new Workspace(device), true);
    setResizable(true, true);
    setResizeLimits(1080, 720, 2560, 1600);
    centreWithSize(1440, 900);
    setVisible(true);
}
void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
} // namespace forge
