#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge
{
class TutorialPanel final : public juce::Component
{
  public:
    TutorialPanel();
    void resized() override;
    void paint(juce::Graphics &) override;

  private:
    void showPage(int);
    int page_ = 0;
    juce::Label heading_, progress_;
    juce::TextEditor body_;
    juce::ComboBox chapters_;
    juce::TextButton back_{"Zurueck"}, next_{"Weiter"};
};
} // namespace forge
