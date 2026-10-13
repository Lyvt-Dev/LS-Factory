#include "LibraryPanel.h"
namespace forge::library
{
LibraryPanel::LibraryPanel(std::function<void(Entry)> preview, std::function<void()> stop,
                           std::function<void(Entry, int)> load, juce::File root)
    : catalog_(std::make_unique<Library>(root)), root_(root == juce::File{} ? Library::defaultRoot() : root),
      preview_(std::move(preview)), stop_(std::move(stop)), load_(std::move(load))
{
    setSize(1040, 700);
    for (auto *c : std::initializer_list<juce::Component *>{&title_,
                                                            &status_,
                                                            &search_,
                                                            &tags_,
                                                            &name_,
                                                            &details_,
                                                            &store_,
                                                            &kind_,
                                                            &category_,
                                                            &genre_,
                                                            &instrument_,
                                                            &mood_,
                                                            &bpm_,
                                                            &view_,
                                                            &destination_,
                                                            &list_,
                                                            &previewButton_,
                                                            &stopButton_,
                                                            &loadButton_,
                                                            &favouriteButton_,
                                                            &collectionButton_,
                                                            &saveButton_,
                                                            &importPack_,
                                                            &exportState_,
                                                            &importState_})
        addAndMakeVisible(c);
    title_.setText("FACTORY / Instruments & Sounds", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(22)));
    search_.setTextToShowWhenEmpty("Search presets, samples, loops and one-shots", juce::Colour(0xff8c9da4));
    tags_.setTextToShowWhenEmpty("Tags (comma separated)", juce::Colour(0xff8c9da4));
    auto values = [&](juce::ComboBox &box, const juce::String &heading, auto getter)
    {
        box.addItem(heading, 1);
        juce::StringArray values;
        for (auto &e : catalog_->entries())
            values.addIfNotAlreadyThere(getter(e));
        values.sort(true);
        for (int i = 0; i < values.size(); ++i)
            box.addItem(values[i], i + 2);
        box.setSelectedId(1, juce::dontSendNotification);
        box.onChange = [this] { refresh(); };
    };
    values(kind_, "All types", [](auto &e) { return e.kind; });
    values(category_, "All categories", [](auto &e) { return e.category; });
    values(genre_, "All genres", [](auto &e) { return e.genre; });
    values(instrument_, "All instruments", [](auto &e) { return e.instrument; });
    values(mood_, "All moods", [](auto &e) { return e.mood; });
    bpm_.addItem("Any BPM", 1);
    bpm_.addItem("90 BPM", 2);
    bpm_.addItem("120 BPM", 3);
    bpm_.addItem("140 BPM", 4);
    bpm_.setEditableText(true);
    bpm_.setSelectedId(1, juce::dontSendNotification);
    bpm_.onChange = [this] { refresh(); };
    for (auto name :
         {"All sounds", "Favourites", "Recent", "Factory / free", "Own library", "Packs / downloads"})
        view_.addItem(name, view_.getNumItems() + 1);
    for (auto &name : catalog_->collections())
        view_.addItem("Collection: " + name, view_.getNumItems() + 1);
    view_.setSelectedId(1, juce::dontSendNotification);
    view_.onChange = [this] { refresh(); };
    destination_.addItem("New instrument track", 1);
    destination_.addItem("Replace selected channel", 2);
    destination_.addItem("Add to current pattern", 3);
    destination_.setSelectedId(1, juce::dontSendNotification);
    search_.onTextChange = [this] { refresh(); };
    tags_.onTextChange = [this] { refresh(); };
    list_.setRowHeight(42);
    details_.setMultiLine(true);
    details_.setReadOnly(true);
    store_.setMultiLine(true);
    store_.setReadOnly(true);
    store_.setVisible(false);
    store_.setText(
        "FREE FACTORY\nLS Factory Essentials is bundled: original presets, samples, one-shots and loops. The "
        "audio content is CC0. All sounds can be previewed before loading.\n\nOWN PACKS\nImport a schema-1 "
        "JSON manifest with audio files next to it. Samples are SHA-256 checked and copied to a permanent "
        "local cache.\n\nOPTIONAL PREMIUM / CLOUD\nNo provider is connected. No premium purchases, "
        "downloads, cloud sync or license claims are made. A CloudProvider interface is ready for catalog, "
        "entitlement and state synchronization.\n\nUse Export / Import favourites to transfer favourites, "
        "recent sounds and collections locally. No personal library data is uploaded.",
        false);
    const char *names[] = {"Volume", "Pan", "Filter", "Delay", "Reverb", "Compression", "Drive"};
    for (size_t i = 0; i < parameters_.size(); ++i)
    {
        addAndMakeVisible(parameters_[i]);
        addAndMakeVisible(parameterNames_[i]);
        parameterNames_[i].setText(names[i], juce::dontSendNotification);
        parameters_[i].setSliderStyle(juce::Slider::LinearHorizontal);
        parameters_[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 24);
        parameters_[i].setRange(i == 1 ? -1 : 0, 1, .01);
        parameters_[i].onValueChange = [this, i]
        {
            if (!edited_)
                return;
            float *fields[] = {&edited_->preset.volume, &edited_->preset.pan,    &edited_->preset.cutoff,
                               &edited_->preset.delay,  &edited_->preset.reverb, &edited_->preset.compression,
                               &edited_->preset.drive};
            *fields[i] = static_cast<float>(parameters_[i].getValue());
        };
    }
    previewButton_.onClick = [this]
    {
        if (edited_)
        {
            preview_(*edited_);
            catalog_->used(edited_->id);
        }
    };
    stopButton_.onClick = stop_;
    loadButton_.onClick = [this]
    {
        if (edited_)
        {
            load_(*edited_, destination_.getSelectedId());
            catalog_->used(edited_->id);
        }
    };
    favouriteButton_.onClick = [this]
    {
        if (edited_)
        {
            catalog_->toggleFavourite(edited_->id);
            favouriteButton_.setToggleState(catalog_->favourite(edited_->id), juce::dontSendNotification);
            list_.repaint();
            if (view_.getSelectedId() == 2)
                refresh();
        }
    };
    collectionButton_.onClick = [this] { collection(); };
    saveButton_.onClick = [this]
    {
        if (edited_)
        {
            auto e = *edited_;
            e.name = name_.getText().trim();
            auto r = catalog_->savePreset(e);
            if (r.wasOk())
            {
                const auto savedIndex = catalog_->entries().size() - 1;
                view_.setSelectedId(5, juce::dontSendNotification);
                refresh();
                const auto found = std::find(rows_.begin(), rows_.end(), savedIndex);
                if (found != rows_.end())
                {
                    const int row = static_cast<int>(found - rows_.begin());
                    list_.selectRow(row);
                    selectedRowsChanged(row);
                }
            }
            status_.setText(r.wasOk() ? "User preset saved" : r.getErrorMessage(),
                            juce::dontSendNotification);
        }
    };
    importPack_.onClick = [this] { chooseFile(true); };
    exportState_.onClick = [this] { chooseFile(false, true); };
    importState_.onClick = [this] { chooseFile(false, false); };
    refresh();
}
LibraryPanel::~LibraryPanel()
{
    worker_.removeAllJobs(true, -1);
    list_.setModel(nullptr);
}
void LibraryPanel::updateChoices()
{
    auto add = [](juce::ComboBox &box, const juce::String &value)
    {
        if (value.isEmpty())
            return;
        for (int i = 0; i < box.getNumItems(); ++i)
            if (box.getItemText(i) == value)
                return;
        box.addItem(value, box.getNumItems() + 1);
    };
    for (auto &e : catalog_->entries())
    {
        add(kind_, e.kind);
        add(category_, e.category);
        add(genre_, e.genre);
        add(instrument_, e.instrument);
        add(mood_, e.mood);
        if (e.bpm > 0)
            add(bpm_, juce::String(e.bpm, 0) + " BPM");
    }
    for (auto &collection : catalog_->collections())
        add(view_, "Collection: " + collection);
}
void LibraryPanel::refresh()
{
    Filter filter;
    filter.text = search_.getText();
    filter.tags = tags_.getText();
    auto selected = [](const juce::ComboBox &box)
    { return box.getSelectedId() > 1 ? box.getText() : juce::String{}; };
    filter.kind = selected(kind_);
    filter.category = selected(category_);
    filter.genre = selected(genre_);
    filter.instrument = selected(instrument_);
    filter.mood = selected(mood_);
    filter.bpm = bpm_.getSelectedId() == 1 ? 0 : bpm_.getText().getDoubleValue();
    filter.favourites = view_.getSelectedId() == 2;
    filter.recent = view_.getSelectedId() == 3;
    if (view_.getSelectedId() > 6)
        filter.collection = view_.getText().fromFirstOccurrenceOf("Collection: ", false, false);
    rows_ = catalog_->search(filter);
    if (view_.getSelectedId() == 4 || view_.getSelectedId() == 5)
    {
        const bool user = view_.getSelectedId() == 5;
        rows_.erase(std::remove_if(rows_.begin(), rows_.end(),
                                   [&](size_t i) { return catalog_->entries()[i].user != user; }),
                    rows_.end());
    }
    const bool store = view_.getSelectedId() == 6;
    store_.setVisible(store);
    list_.setVisible(!store);
    list_.updateContent();
    edited_.reset();
    if (!rows_.empty() && !store)
    {
        list_.selectRow(0);
        selectedRowsChanged(0);
    }
    else
    {
        list_.deselectAllRows();
        details_.setText("No matching sounds", false);
    }
    for (auto *button : {&previewButton_, &loadButton_, &saveButton_, &favouriteButton_, &collectionButton_})
        button->setEnabled(!store && !rows_.empty());
    for (auto &parameter : parameters_)
        parameter.setEnabled(!store && !rows_.empty());
    name_.setReadOnly(store);
    if (store)
    {
        name_.setText("Packs / downloads", false);
        details_.setText("Free original Factory Essentials are included. Own packs can be imported locally. "
                         "Premium/cloud has no connected provider.",
                         false);
    }
    status_.setText(juce::String(rows_.size()) +
                        " sounds / local library / drag a sound to the Rack or Playlist",
                    juce::dontSendNotification);
}
int LibraryPanel::getNumRows()
{
    return static_cast<int>(rows_.size());
}
void LibraryPanel::paintListBoxItem(int row, juce::Graphics &g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    const auto &e = catalog_->entries()[rows_[static_cast<size_t>(row)]];
    g.fillAll(juce::Colour(selected ? 0xff46544b : 0xff29343a));
    g.setColour(juce::Colour(0xffd5dddf));
    g.setFont(juce::Font(juce::FontOptions(14)));
    g.drawText((catalog_->favourite(e.id) ? "* " : "") + e.name, juce::Rectangle<int>{12, 3, width - 24, 20},
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff8c9da4));
    g.setFont(juce::Font(juce::FontOptions(11)));
    g.drawText(e.category + " / " + e.kind + " / " + e.genre +
                   (e.bpm > 0 ? " / " + juce::String(e.bpm, 0) + " BPM" : ""),
               juce::Rectangle<int>{12, 23, width - 24, height - 23}, juce::Justification::centredLeft);
}
void LibraryPanel::selectedRowsChanged(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size()))
    {
        edited_.reset();
        return;
    }
    edited_ = catalog_->entries()[rows_[static_cast<size_t>(row)]];
    details();
}
void LibraryPanel::details()
{
    if (!edited_)
        return;
    const auto &e = *edited_;
    name_.setText(e.name, false);
    details_.setText(
        e.pack + " / " + e.license + "\n" + e.description + "\nTags: " + e.tags.joinIntoString(", ") +
            "\nInstrument: " + e.instrument + " / Engine: " +
            juce::String(e.preset.instrument == InstrumentType::Sampler ? "Sampler" : "Native synthesis"),
        false);
    const float fields[] = {e.preset.volume, e.preset.pan,         e.preset.cutoff, e.preset.delay,
                            e.preset.reverb, e.preset.compression, e.preset.drive};
    for (size_t i = 0; i < parameters_.size(); ++i)
        parameters_[i].setValue(fields[i], juce::dontSendNotification);
    favouriteButton_.setToggleState(catalog_->favourite(e.id), juce::dontSendNotification);
}
void LibraryPanel::listBoxItemDoubleClicked(int, const juce::MouseEvent &)
{
    previewButton_.onClick();
}
juce::var LibraryPanel::getDragSourceDescription(const juce::SparseSet<int> &rows)
{
    if (rows.size() != 1)
        return {};
    const int row = rows[0];
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return {};
    return "lsfactory-sound:" + catalog_->entries()[rows_[static_cast<size_t>(row)]].id;
}
std::optional<Entry> LibraryPanel::lookup(const juce::String &id) const
{
    if (edited_ && edited_->id == id)
        return edited_;
    for (auto &e : catalog_->entries())
        if (e.id == id)
            return e;
    return {};
}
void LibraryPanel::collection()
{
    if (!edited_)
        return;
    auto *dialog = new juce::AlertWindow("Add to collection", "Choose a collection name",
                                         juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", "My collection", "Name");
    dialog->addButton("Add", 1);
    dialog->addButton("Cancel", 0);
    const auto id = edited_->id;
    const juce::Component::SafePointer<LibraryPanel> self(this);
    const juce::Component::SafePointer<juce::AlertWindow> alert(dialog);
    dialog->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [self, alert, id](int result)
            {
                if (self && alert && result == 1)
                {
                    const auto name = alert->getTextEditorContents("name");
                    const auto r = self->catalog_->collect(name, id);
                    self->status_.setText(r.wasOk() ? "Added to collection" : r.getErrorMessage(),
                                          juce::dontSendNotification);
                    bool found = false;
                    for (int i = 0; i < self->view_.getNumItems(); ++i)
                        if (self->view_.getItemText(i) == "Collection: " + name)
                            found = true;
                    if (!found)
                        self->view_.addItem("Collection: " + name, self->view_.getNumItems() + 1);
                }
            }),
        true);
}
void LibraryPanel::chooseFile(bool pack, bool exportState)
{
    chooser_ = std::make_unique<juce::FileChooser>(pack          ? "Import sound pack manifest"
                                                   : exportState ? "Export library state"
                                                                 : "Import library state",
                                                   root_, "*.json");
    const juce::Component::SafePointer<LibraryPanel> self(this);
    chooser_->launchAsync(
        (exportState ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode) |
            juce::FileBrowserComponent::canSelectFiles,
        [self, pack, exportState](const juce::FileChooser &chooser)
        {
            if (!self || chooser.getResult() == juce::File{})
                return;
            const auto file = chooser.getResult();
            if (!pack)
            {
                auto r = exportState ? self->catalog_->exportState(file) : self->catalog_->importState(file);
                self->refresh();
                self->status_.setText(r.wasOk() ? "Library state transferred" : r.getErrorMessage(),
                                      juce::dontSendNotification);
                return;
            }
            self->status_.setText("Validating and importing pack...", juce::dontSendNotification);
            const auto root = self->root_;
            self->worker_.addJob(
                [self, file, root]
                {
                    Library imported(root);
                    const auto r = imported.importPack(file);
                    juce::MessageManager::callAsync(
                        [self, r, root]
                        {
                            if (!self)
                                return;
                            if (r.wasOk())
                            {
                                self->catalog_ = std::make_unique<Library>(root);
                                self->updateChoices();
                            }
                            self->refresh();
                            self->status_.setText(r.wasOk() ? "Pack imported to own library"
                                                            : r.getErrorMessage(),
                                                  juce::dontSendNotification);
                        });
                });
        });
}
void LibraryPanel::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff303a40));
}
void LibraryPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    title_.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);
    auto row = area.removeFromTop(30);
    search_.setBounds(row.removeFromLeft(row.getWidth() * 2 / 3).reduced(0, 0));
    row.removeFromLeft(12);
    view_.setBounds(row);
    area.removeFromTop(10);
    row = area.removeFromTop(28);
    const int width = row.getWidth() / 6;
    for (auto *box : {&kind_, &category_, &genre_, &instrument_, &mood_, &bpm_})
        box->setBounds(row.removeFromLeft(width).reduced(3, 0));
    area.removeFromTop(10);
    tags_.setBounds(area.removeFromTop(26));
    area.removeFromTop(12);
    status_.setBounds(area.removeFromBottom(24));
    auto bottom = area.removeFromBottom(32);
    importPack_.setBounds(bottom.removeFromLeft(160));
    bottom.removeFromLeft(12);
    exportState_.setBounds(bottom.removeFromLeft(160));
    bottom.removeFromLeft(12);
    importState_.setBounds(bottom.removeFromLeft(160));
    area.removeFromBottom(14);
    auto left = area.removeFromLeft(area.getWidth() / 2);
    list_.setBounds(left);
    store_.setBounds(left);
    area.removeFromLeft(18);
    name_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);
    details_.setBounds(area.removeFromTop(64));
    area.removeFromTop(8);
    row = area.removeFromTop(30);
    previewButton_.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(8);
    stopButton_.setBounds(row.removeFromLeft(80));
    row.removeFromLeft(8);
    favouriteButton_.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(8);
    collectionButton_.setBounds(row);
    for (size_t i = 0; i < parameters_.size(); ++i)
    {
        row = area.removeFromTop(24);
        parameterNames_[i].setBounds(row.removeFromLeft(90));
        parameters_[i].setBounds(row);
    }
    area.removeFromTop(8);
    destination_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);
    row = area.removeFromTop(30);
    loadButton_.setBounds(row.removeFromLeft(180));
    row.removeFromLeft(12);
    saveButton_.setBounds(row);
}
} // namespace forge::library
