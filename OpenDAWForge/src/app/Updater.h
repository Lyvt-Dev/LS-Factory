#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge
{
struct UpdateRelease
{
    juce::String version, filename, url, sha256;
    juce::int64 size = 0;
};
class UpdateService
{
  public:
    static juce::Result selectRelease(const juce::var &, const juce::String &current, UpdateRelease &);
    static juce::Result check(const juce::String &current, UpdateRelease &);
    static juce::Result download(const UpdateRelease &, const juce::File &directory, juce::File &);
    static juce::Result verifyFile(const UpdateRelease &, const juce::File &);
    static bool smokeTest(const juce::File &directory);
};
class UpdatePanel final : public juce::Component, private juce::Timer
{
  public:
    UpdatePanel(std::function<bool()> canInstall, bool automaticChecks = true, juce::File preferences = {});
    ~UpdatePanel() override;
    void resized() override;
    void paint(juce::Graphics &) override;
    bool preferencesSmokeTest();
    juce::String availableVersion() const
    {
        return release_.version;
    }

  private:
    void timerCallback() override;
    void check();
    void download();
    void apply();
    void saveSettings();
    std::function<bool()> canInstall_;
    juce::File preferences_, installer_;
    juce::ThreadPool worker_{1};
    UpdateRelease release_;
    bool busy_ = false, started_ = false, automaticChecks_ = true, autoAttempted_ = false;
    juce::Label title_, status_;
    juce::ToggleButton checkAtStart_{"Beim Start nach Updates suchen"},
        installAutomatically_{
            "Updates automatisch installieren (wenn Projekt gespeichert und Wiedergabe gestoppt)"};
    juce::TextButton checkNow_{"Jetzt pruefen"}, installNow_{"Update installieren"},
        releasePage_{"Release-Seite"};
};
} // namespace forge
