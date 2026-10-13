#pragma once
#include "Library.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge::library
{
class LoopStarter final : public juce::Component, public juce::ListBoxModel
{
  public:
    LoopStarter(std::function<void(Idea)> preview, std::function<void(Entry)> sound,
                std::function<void()> stop, std::function<void(Idea, bool, bool)> load, juce::File root = {});
    void paint(juce::Graphics &) override;
    void resized() override;
    int getNumRows() override
    {
        return static_cast<int>(idea_.parts.size());
    }
    void paintListBoxItem(int, juce::Graphics &, int, int, bool) override;

  private:
    void generate();
    Library library_;
    Idea idea_;
    int seed_ = 0;
    std::function<void(Idea)> preview_;
    std::function<void(Entry)> sound_;
    std::function<void()> stop_;
    std::function<void(Idea, bool, bool)> load_;
    juce::Label title_, status_;
    juce::ComboBox genre_, root_, scale_, mood_;
    juce::Slider bpm_;
    juce::ListBox parts_{"Idea parts", this};
    juce::TextButton generate_{"Generate idea"}, replace_{"Replace selected loop"},
        previewButton_{"Preview idea"}, soundButton_{"Preview selected"}, stopButton_{"Stop"},
        pattern_{"Add to pattern"}, arrangement_{"Add to arrangement"};
    juce::ToggleButton tempo_{"Adopt this BPM in the project (otherwise keep project tempo)"};
};
} // namespace forge::library
