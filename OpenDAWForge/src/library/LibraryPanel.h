#pragma once
#include "Library.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace forge::library
{
class LibraryPanel final : public juce::Component,
                           public juce::ListBoxModel,
                           public juce::DragAndDropContainer
{
  public:
    LibraryPanel(std::function<void(Entry)> preview, std::function<void()> stop,
                 std::function<void(Entry, int)> load, juce::File root = {});
    ~LibraryPanel() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics &, int, int, bool) override;
    void selectedRowsChanged(int) override;
    void listBoxItemDoubleClicked(int, const juce::MouseEvent &) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int> &) override;
    std::optional<Entry> lookup(const juce::String &) const;

  private:
    void refresh();
    void updateChoices();
    void details();
    void chooseFile(bool importPack, bool exportState = false);
    void collection();
    std::unique_ptr<Library> catalog_;
    juce::File root_;
    std::vector<size_t> rows_;
    std::optional<Entry> edited_;
    std::function<void(Entry)> preview_;
    std::function<void()> stop_;
    std::function<void(Entry, int)> load_;
    juce::TextEditor search_, tags_, name_, details_, store_;
    juce::ComboBox kind_, category_, genre_, instrument_, mood_, bpm_, view_, destination_;
    juce::ListBox list_{"Factory library", this};
    std::array<juce::Slider, 7> parameters_;
    std::array<juce::Label, 7> parameterNames_;
    juce::TextButton previewButton_{"Preview"}, stopButton_{"Stop"}, loadButton_{"Load to project"},
        favouriteButton_{"Favourite"}, collectionButton_{"+ Collection"}, saveButton_{"Save preset"},
        importPack_{"Import sound pack"}, exportState_{"Export favourites"},
        importState_{"Import favourites"};
    juce::Label title_, status_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::ThreadPool worker_{1};
};
} // namespace forge::library
