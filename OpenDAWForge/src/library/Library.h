#pragma once
#include "model/Project.h"
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
namespace forge::library
{
struct Entry
{
    Entry() = default;
    juce::String id, name, kind, category, genre, instrument, mood, description, license, pack, resource,
        sha256, file;
    juce::StringArray tags;
    double bpm = 0, beats = 4;
    Channel preset;
    bool user = false;
};
struct Filter
{
    juce::String text, kind, category, genre, instrument, mood, tags, collection;
    double bpm = 0;
    bool favourites = false, recent = false;
};
struct IdeaPart
{
    Entry entry;
    std::vector<MidiNote> notes;
};
struct Idea
{
    double bpm = 120;
    int root = 60;
    bool minor = true;
    std::vector<IdeaPart> parts;
};
// Optional provider seam: no account, license entitlement or network synchronization is fabricated.
class CloudProvider
{
  public:
    virtual ~CloudProvider() = default;
    virtual juce::Result catalog(juce::var &) = 0;
    virtual juce::Result downloadPack(const juce::String &, const juce::File &) = 0;
    virtual juce::Result synchronize(const juce::var &local, juce::var &merged) = 0;
    virtual juce::Result entitlement(const juce::String &pack, bool &licensed) = 0;
};
class Library
{
  public:
    explicit Library(juce::File root = {});
    const std::vector<Entry> &entries() const
    {
        return entries_;
    }
    std::vector<size_t> search(const Filter &) const;
    bool favourite(const juce::String &) const;
    void toggleFavourite(const juce::String &);
    void used(const juce::String &);
    juce::StringArray collections() const;
    juce::Result collect(const juce::String &collection, const juce::String &id);
    juce::Result savePreset(Entry);
    juce::Result exportState(const juce::File &) const;
    juce::Result importState(const juce::File &);
    juce::Result importPack(const juce::File &manifest);
    static juce::Result materialize(const Entry &, juce::File &, juce::File root = {});
    Idea generate(const juce::String &genre, double bpm, int root, bool minor, const juce::String &mood,
                  int seed) const;
    void replace(Idea &, size_t part, const juce::String &genre, const juce::String &mood, int seed) const;
    static juce::Result applyIdea(Project &, const Idea &, bool arrangement, bool adoptTempo,
                                  juce::File cacheRoot = {});
    static bool smokeTest(const juce::File &);
    static juce::File defaultRoot();

  private:
    std::vector<Entry> entries_;
    juce::File root_;
    juce::var state_;
    void saveState();
    void loadEntries(const juce::var &, bool user);
};
} // namespace forge::library
