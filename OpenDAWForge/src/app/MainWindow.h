#pragma once
#include "commands/UndoStack.h"
#include "engine/AudioEngine.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge
{
class Workspace final : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::AudioProcessorListener,
                        private juce::Timer
{
  public:
    explicit Workspace(bool openDevice = true);
    ~Workspace() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
    bool keyPressed(const juce::KeyPress &) override;
    bool isInterestedInFileDrag(const juce::StringArray &) override;
    void filesDropped(const juce::StringArray &, int, int) override;
    bool smokeTest(const juce::File &directory);

  private:
    enum class View
    {
        Playlist,
        Piano,
        Mixer
    };
    Project project_ = makeDemoProject();
    UndoStack undo_;
    AudioEngine audio_;
    juce::TextButton play_{"PLAY"}, stop_{"STOP"}, song_{"SONG"}, loop_{"LOOP"}, save_{"SAVE"}, open_{"OPEN"},
        export_{"EXPORT WAV"}, settings_{"AUDIO"}, midiRecord_{"MIDI REC"}, audioRecord_{"REC IN"},
        pluginsButton_{"INSERTS"};
    juce::TextButton playlist_{"Playlist  F5"}, piano_{"Piano roll  F7"}, mixer_{"Mixer  F9"},
        rackButton_{"Channel rack  F6"}, undoButton_{"UNDO"}, redoButton_{"REDO"};
    juce::Slider tempo_;
    juce::ComboBox patterns_, channels_;
    juce::TextEditor search_;
    juce::LookAndFeel_V4 look_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::ThreadPool workers_{1};
    std::unique_ptr<juce::DocumentWindow> pluginWindow_;
    bool pluginsLoading_ = false;
    std::atomic<bool> pluginEdited_{false};
    juce::AudioProcessor *listeningProcessor_ = nullptr;
    void closePluginEditor();
    void audioProcessorParameterChanged(juce::AudioProcessor *, int, float) override
    {
        pluginEdited_.store(true);
    }
    void audioProcessorChanged(juce::AudioProcessor *,
                               const juce::AudioProcessorListener::ChangeDetails &) override
    {
        pluginEdited_.store(true);
    }
    void showPlugins();
    void choosePlugin();
    void applyPluginChain(const std::vector<PluginSlot> &slots, std::string description);
    PatternID selected_;
    int selectedChannel_ = 0;
    View view_ = View::Playlist;
    bool songMode_ = true, showRack_ = true;
    juce::String message_ = juce::String::fromUTF8("Ready · Space to play · Click a step to sequence");
    MidiRecorder midiRecorder_;
    PatternID recordPattern_;
    juce::File currentFile_;
    bool dirty_ = false;
    int autosaveTicks_ = 0;
    float beatWidth_ = 28;
    int pianoTop_ = 84;
    int draggedNote_ = -1, dragTrack_ = -1, dragClip_ = -1, dragFader_ = -1;
    bool resizingNote_ = false, resizingClip_ = false;
    double dragBeat_ = 0;
    int dragPitch_ = 0;
    Project beforeDrag_;
    std::optional<Project> preview_;
    PlaybackSnapshot pendingSnapshot_;
    bool pendingAudio_ = false;
    int rackOffset_ = 0, mixerOffset_ = 0;
    const Project &displayProject() const noexcept
    {
        return preview_ ? *preview_ : project_;
    }
    void restoreSamplesAsync();
    juce::TooltipWindow tooltips_{this, 500};
    juce::Rectangle<int> browserBounds_, editorBounds_, rackBounds_;
    void timerCallback() override;
    void sync(bool refreshSelectors = false);
    void edit(const std::string &, const std::function<void(Project &)> &);
    Pattern *activePattern();
    const Pattern *activePattern() const;
    void drawBrowser(juce::Graphics &);
    void drawPlaylist(juce::Graphics &);
    void drawPiano(juce::Graphics &);
    void drawMixer(juce::Graphics &);
    void drawRack(juce::Graphics &);
    void selectView(View);
    void chooseSave();
    void chooseOpen();
    void chooseExport();
    void showSettings();
    void toggleMidiRecording();
    void toggleAudioRecording();
    void importSample(const juce::File &, int channel);
    void saveTo(const juce::File &, bool autosave = false);
    void transportToggle();
    juce::Rectangle<int> timeline() const;
};
class MainWindow final : public juce::DocumentWindow
{
  public:
    explicit MainWindow(bool openDevice = true);
    void closeButtonPressed() override;
    Workspace &workspace()
    {
        return *static_cast<Workspace *>(getContentComponent());
    }
};
} // namespace forge
