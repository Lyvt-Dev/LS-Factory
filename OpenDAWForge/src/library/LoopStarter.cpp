#include "LoopStarter.h"
namespace forge::library
{
LoopStarter::LoopStarter(std::function<void(Idea)> preview, std::function<void(Entry)> sound,
                         std::function<void()> stop, std::function<void(Idea, bool, bool)> load,
                         juce::File root)
    : library_(root), preview_(std::move(preview)), sound_(std::move(sound)), stop_(std::move(stop)),
      load_(std::move(load))
{
    setSize(760, 570);
    for (auto *c : std::initializer_list<juce::Component *>{
             &title_, &status_, &genre_, &root_, &scale_, &mood_, &bpm_, &parts_, &generate_, &replace_,
             &previewButton_, &soundButton_, &stopButton_, &pattern_, &arrangement_, &tempo_})
        addAndMakeVisible(c);
    title_.setText("LOOP STARTER / Factory Ideas", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(23)));
    int i = 1;
    for (auto name : {"House", "Techno", "Hip-hop", "Ambient", "Electronic"})
        genre_.addItem(name, i++);
    genre_.setSelectedId(1, juce::dontSendNotification);
    i = 1;
    for (auto name : {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"})
        root_.addItem(name, i++);
    root_.setSelectedId(1, juce::dontSendNotification);
    scale_.addItem("Minor", 1);
    scale_.addItem("Major", 2);
    scale_.setSelectedId(1, juce::dontSendNotification);
    i = 1;
    for (auto name : {"Energetic", "Dreamy", "Relaxed"})
        mood_.addItem(name, i++);
    mood_.setSelectedId(1, juce::dontSendNotification);
    bpm_.setRange(20, 400, 1);
    bpm_.setSliderStyle(juce::Slider::IncDecButtons);
    bpm_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 64, 28);
    bpm_.setValue(120, juce::dontSendNotification);
    bpm_.setTextValueSuffix(" BPM");
    parts_.setRowHeight(50);
    generate_.onClick = [this] { generate(); };
    replace_.onClick = [this]
    {
        const int part = parts_.getSelectedRow();
        if (part >= 0)
        {
            auto fresh =
                library_.generate(genre_.getText(), idea_.bpm, idea_.root, idea_.minor, mood_.getText(),
                                  seed_ = juce::Random::getSystemRandom().nextInt());
            if (static_cast<size_t>(part) < fresh.parts.size())
            {
                const auto source = idea_.parts[static_cast<size_t>(part)].entry;
                idea_.parts[static_cast<size_t>(part)] = fresh.parts[static_cast<size_t>(part)];
                idea_.parts[static_cast<size_t>(part)].entry = source;
                library_.replace(idea_, static_cast<size_t>(part), genre_.getText(), mood_.getText(), seed_);
            }
            parts_.repaint();
        }
    };
    previewButton_.onClick = [this] { preview_(idea_); };
    soundButton_.onClick = [this]
    {
        const int row = parts_.getSelectedRow();
        if (row >= 0 && row < static_cast<int>(idea_.parts.size()))
            sound_(idea_.parts[static_cast<size_t>(row)].entry);
    };
    stopButton_.onClick = stop_;
    pattern_.onClick = [this] { load_(idea_, false, tempo_.getToggleState()); };
    arrangement_.onClick = [this] { load_(idea_, true, tempo_.getToggleState()); };
    generate();
}
void LoopStarter::generate()
{
    seed_ = juce::Random::getSystemRandom().nextInt();
    idea_ = library_.generate(genre_.getText(), bpm_.getValue(), 59 + root_.getSelectedId(),
                              scale_.getSelectedId() == 1, mood_.getText(), seed_);
    parts_.updateContent();
    if (!idea_.parts.empty())
        parts_.selectRow(0);
    status_.setText("8-beat MIDI loop / " + genre_.getText() + " / " + root_.getText() + " " +
                        scale_.getText() + " / seed " + juce::String(seed_) +
                        ". Audio drum loops are also in Factory > loop, filtered by BPM.",
                    juce::dontSendNotification);
}
void LoopStarter::paintListBoxItem(int row, juce::Graphics &g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int>(idea_.parts.size()))
        return;
    const auto &part = idea_.parts[static_cast<size_t>(row)];
    g.fillAll(juce::Colour(selected ? 0xff46544b : 0xff29343a));
    g.setColour(juce::Colour(0xffd5dddf));
    g.setFont(juce::Font(juce::FontOptions(15)));
    g.drawText(part.entry.name, juce::Rectangle<int>{12, 4, width - 24, 22},
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff8c9da4));
    g.setFont(juce::Font(juce::FontOptions(12)));
    g.drawText(part.entry.category + " / " + juce::String(part.notes.size()) + " notes / " +
                   part.entry.instrument,
               juce::Rectangle<int>{12, 28, width - 24, height - 28}, juce::Justification::centredLeft);
}
void LoopStarter::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff303a40));
}
void LoopStarter::resized()
{
    auto area = getLocalBounds().reduced(24);
    title_.setBounds(area.removeFromTop(36));
    area.removeFromTop(14);
    auto row = area.removeFromTop(30);
    genre_.setBounds(row.removeFromLeft(130));
    row.removeFromLeft(10);
    bpm_.setBounds(row.removeFromLeft(112));
    row.removeFromLeft(10);
    root_.setBounds(row.removeFromLeft(70));
    row.removeFromLeft(10);
    scale_.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(10);
    mood_.setBounds(row);
    area.removeFromTop(14);
    row = area.removeFromTop(32);
    generate_.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(12);
    replace_.setBounds(row.removeFromLeft(230));
    area.removeFromTop(14);
    auto bottom = area.removeFromBottom(136);
    tempo_.setBounds(bottom.removeFromTop(28));
    bottom.removeFromTop(8);
    row = bottom.removeFromTop(32);
    previewButton_.setBounds(row.removeFromLeft(140));
    row.removeFromLeft(8);
    soundButton_.setBounds(row.removeFromLeft(160));
    row.removeFromLeft(8);
    stopButton_.setBounds(row.removeFromLeft(80));
    bottom.removeFromTop(10);
    row = bottom.removeFromTop(32);
    pattern_.setBounds(row.removeFromLeft(180));
    row.removeFromLeft(12);
    arrangement_.setBounds(row.removeFromLeft(200));
    status_.setBounds(bottom);
    parts_.setBounds(area);
}
} // namespace forge::library
