#pragma once
#include "commands/UndoStack.h"
#include "engine/AudioEngine.h"
#include "library/Library.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge
{
class Workspace final : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        public juce::DragAndDropTarget,
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
    struct FactorySmoke
    {
        Project before;
        int phase = 0;
        double deadline = 0;
        bool finished = false;
    };
    bool factoryIntegrationSmokeStep(FactorySmoke &);

    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails &) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails &) override;
    void openProject(const juce::File &);

  private:
    enum class View
    {
        Playlist,
        Piano,
        Mixer
    };
    Project project_;
    UndoStack undo_;
    AudioEngine audio_;
    juce::TextButton play_{"PLAY"}, stop_{"STOP"}, song_{"SONG"}, loop_{"LOOP"}, save_{"SAVE"}, open_{"OPEN"},
        export_{"EXPORT WAV"}, settings_{"AUDIO"}, midiRecord_{"MIDI REC"}, audioRecord_{"REC IN"},
        pluginsButton_{"INSERTS"};
    juce::TextButton playlist_{"Playlist  F5"}, piano_{"Piano roll  F7"}, mixer_{"Mixer  F9"},
        rackButton_{"Channel rack  F6"}, undoButton_{"UNDO"}, redoButton_{"REDO"};
    juce::TextButton newProject_{"PROJECT"}, addTrack_{"+ TRACK"}, help_{"HELP  F1"}, layout_{"LAYOUT"},
        fullscreen_{"FULL  F11"}, updates_{"SETTINGS"}, factory_{"FACTORY"}, ideas_{"LOOP STARTER"};
    std::unique_ptr<juce::DocumentWindow> tutorialWindow_;
    void showTutorial();
    void showLibrary();
    void showLoopStarter();
    void auditionEntry(library::Entry);
    void auditionIdea(library::Idea);
    void loadLibraryEntry(library::Entry, int destination, int track = -1, double beat = 0);
    void loadIdea(library::Idea, bool arrangement, bool tempo);
    std::unique_ptr<juce::DocumentWindow> libraryWindow_, ideaWindow_;
    juce::File libraryRoot_ = library::Library::defaultRoot();
    void showUpdates(bool visible = true);
    std::unique_ptr<juce::DocumentWindow> updateWindow_;
    bool readyForUpdate() const;
    void projectMenu();
    void replaceProject(bool demo);
    void addTrack(InstrumentType);
    void resetLayout();
    juce::Rectangle<int> constrainPanel(juce::Rectangle<int>, bool rack) const;
    std::array<juce::Rectangle<int>, 3> viewBounds_{};
    bool rackPlaced_ = false;
    int panelDrag_ = 0;
    bool panelResize_ = false;
    juce::Rectangle<int> panelBefore_;
    juce::Slider tempo_;
    juce::ComboBox patterns_, channels_;
    juce::TextEditor search_;
    juce::LookAndFeel_V4 look_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::ThreadPool workers_{1};
    std::unique_ptr<juce::DocumentWindow> pluginWindow_;
    bool pluginsLoading_ = false, factoryLoading_ = false;
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
    juce::String message_ =
        juce::String::fromUTF8("Empty project · + TRACK to begin · HELP for the tutorial");
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
    void restoreSamplesAsync(bool preservePosition = false);
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
    void previewNote(int pitch, float velocity = 0.8f);
    bool stopPreview();
    int previewPitch_ = -1, previewChannel_ = 0;
    double previewUntil_ = 0;
    juce::Rectangle<int> timeline() const;
};
class MainWindow final : public juce::DocumentWindow
{
  public:
    explicit MainWindow(bool openDevice = true);
    void closeButtonPressed() override;
    void toggleFullscreen();
    Workspace &workspace()
    {
        return *static_cast<Workspace *>(getContentComponent());
    }
};
} // namespace forge
