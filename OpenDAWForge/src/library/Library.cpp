#include "Library.h"
#include "FactoryAssets.h"
#include "engine/Scheduler.h"
#include "persistence/BinaryFormat.h"
#include <cmath>
#include <iostream>
#include <random>
namespace forge::library
{
namespace
{
Entry decode(const juce::var &v, bool user)
{
    Entry e;
    e.id = v["id"].toString();
    e.name = v["name"].toString();
    e.kind = v["kind"].toString();
    e.category = v["category"].toString();
    e.genre = v["genre"].toString();
    e.instrument = v["instrument"].toString();
    e.mood = v["mood"].toString();
    e.description = v["description"].toString();
    e.license = v["license"].toString();
    e.pack = v["pack"].toString();
    e.resource = v["resource"].toString();
    e.file = v["file"].toString();
    e.sha256 = v["sha256"].toString();
    e.bpm = static_cast<double>(v["bpm"]);
    e.beats = v.hasProperty("beats") ? static_cast<double>(v["beats"]) : 4;
    e.user = user;
    if (e.genre.isEmpty())
        e.genre = "Unclassified";
    if (e.mood.isEmpty())
        e.mood = "Neutral";
    if (e.instrument.isEmpty())
        e.instrument = "Native instrument";
    if (auto *tags = v["tags"].getArray())
        for (auto &tag : *tags)
            if (e.tags.size() < 32)
                e.tags.add(tag.toString().substring(0, 64));
    const auto p = v["parameters"];
    e.preset.name = e.name.toStdString();
    e.preset.color = e.category == "Bass"     ? 0xffb3a56f
                     : e.category == "Synths" ? 0xffb797c7
                     : e.category == "Keys"   ? 0xff80aab5
                     : e.category == "Pads"   ? 0xff8ba6c2
                     : e.category == "FX"     ? 0xffbf7e79
                     : e.category == "Vocals" ? 0xffb99a80
                                              : 0xff98b778;
    e.preset.instrument = static_cast<InstrumentType>(static_cast<int>(p["type"]));
    auto value = [&](const char *name, float fallback)
    { return p.hasProperty(name) ? static_cast<float>(p[name]) : fallback; };
    e.preset.volume = value("volume", .65f);
    e.preset.pan = value("pan", 0);
    e.preset.cutoff = value("cutoff", .6f);
    e.preset.delay = value("delay", 0);
    e.preset.reverb = value("reverb", 0);
    e.preset.compression = value("compression", 0);
    e.preset.drive = value("drive", 0);
    return e;
}
juce::var encode(const Entry &e)
{
    auto v = std::make_unique<juce::DynamicObject>();
    for (auto pair :
         std::initializer_list<std::pair<const char *, juce::String>>{{"id", e.id},
                                                                      {"name", e.name},
                                                                      {"kind", e.kind},
                                                                      {"category", e.category},
                                                                      {"genre", e.genre},
                                                                      {"instrument", e.instrument},
                                                                      {"mood", e.mood},
                                                                      {"description", e.description},
                                                                      {"license", e.license},
                                                                      {"pack", e.pack},
                                                                      {"resource", e.resource},
                                                                      {"sha256", e.sha256},
                                                                      {"file", e.file}})
        v->setProperty(pair.first, pair.second);
    v->setProperty("bpm", e.bpm);
    v->setProperty("beats", e.beats);
    v->setProperty("tier", "free");
    juce::Array<juce::var> tags;
    for (auto &tag : e.tags)
        tags.add(tag);
    v->setProperty("tags", tags);
    auto p = std::make_unique<juce::DynamicObject>();
    p->setProperty("type", static_cast<int>(e.preset.instrument));
    p->setProperty("volume", e.preset.volume);
    p->setProperty("pan", e.preset.pan);
    p->setProperty("cutoff", e.preset.cutoff);
    p->setProperty("delay", e.preset.delay);
    p->setProperty("reverb", e.preset.reverb);
    p->setProperty("compression", e.preset.compression);
    p->setProperty("drive", e.preset.drive);
    v->setProperty("parameters", juce::var(p.release()));
    return juce::var(v.release());
}
bool valid(const Entry &e)
{
    if (!std::isfinite(e.beats) || e.beats < .25 || e.beats > 128 || (e.kind == "loop" && e.bpm <= 0))
        return false;
    if (e.genre.length() > 128 || e.mood.length() > 128 || e.category.isEmpty() ||
        e.category.length() > 128 || e.instrument.length() > 128 || e.pack.length() > 256 ||
        e.description.length() > 4096 ||
        (e.kind != "preset" && e.kind != "sample" && e.kind != "one-shot" && e.kind != "loop"))
        return false;
    if (e.id.isEmpty() || e.id.length() > 128 || e.name.isEmpty() || e.name.length() > 256 ||
        e.file.length() > 4096 || e.resource.length() > 256 || !std::isfinite(e.bpm) || e.bpm < 0 ||
        e.bpm > 400)
        return false;
    Project p;
    auto c = e.preset;
    c.id = ChannelID{1};
    p.channels.push_back(c);
    return static_cast<bool>(p.validate());
}
juce::var object()
{
    return juce::var(new juce::DynamicObject);
}
juce::StringArray strings(const juce::var &v)
{
    juce::StringArray out;
    if (auto *array = v.getArray())
        for (const auto &x : *array)
            if (out.size() < 4096 && x.toString().length() <= 128)
                out.addIfNotAlreadyThere(x.toString());
    return out;
}
juce::var array(const juce::StringArray &s)
{
    juce::Array<juce::var> a;
    for (auto &x : s)
        a.add(x);
    return juce::var(a);
}
} // namespace
juce::File Library::defaultRoot()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("LSFactory/library");
}
Library::Library(juce::File root) : root_(root == juce::File{} ? defaultRoot() : root)
{
    int size = 0;
    const auto *data = FactoryAssets::getNamedResource("manifest_json", size);
    if (data)
        loadEntries(juce::JSON::parse(juce::String::fromUTF8(data, size))["entries"], false);
    loadEntries(juce::JSON::parse(root_.getChildFile("user-library.json")), true);
    state_ = juce::JSON::parse(root_.getChildFile("state.json"));
    if (!state_.isObject())
        state_ = object();
}
void Library::loadEntries(const juce::var &v, bool user)
{
    if (const auto *a = v.getArray())
        for (const auto &row : *a)
        {
            if (entries_.size() >= 1024)
                break;
            auto e = decode(row, user);
            if (valid(e) &&
                std::none_of(entries_.begin(), entries_.end(), [&](const auto &x) { return x.id == e.id; }))
                entries_.push_back(e);
        }
}
std::vector<size_t> Library::search(const Filter &f) const
{
    std::vector<size_t> out;
    const auto recents = strings(state_["recent"]);
    const auto collection = f.collection.isEmpty()
                                ? juce::StringArray{}
                                : strings(state_["collections"][juce::Identifier(f.collection)]);
    const auto tags = juce::StringArray::fromTokens(f.tags.toLowerCase(), ", ", "");
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        const auto &e = entries_[i];
        const auto hay =
            (e.name + " " + e.description + " " + e.tags.joinIntoString(" ") + " " + e.instrument)
                .toLowerCase();
        if (f.text.isNotEmpty() && !hay.contains(f.text.toLowerCase()))
            continue;
        if ((f.kind.isNotEmpty() && e.kind != f.kind) ||
            (f.category.isNotEmpty() && e.category != f.category) ||
            (f.genre.isNotEmpty() && e.genre != f.genre) ||
            (f.instrument.isNotEmpty() && e.instrument != f.instrument) ||
            (f.mood.isNotEmpty() && e.mood != f.mood) ||
            (f.bpm > 0 && (e.bpm <= 0 || std::abs(e.bpm - f.bpm) > 1)))
            continue;
        if (f.favourites && !favourite(e.id))
            continue;
        if (f.recent && !recents.contains(e.id))
            continue;
        if (f.collection.isNotEmpty() && !collection.contains(e.id))
            continue;
        if (std::any_of(tags.begin(), tags.end(),
                        [&](const auto &tag) { return tag.isNotEmpty() && !e.tags.contains(tag, true); }))
            continue;
        out.push_back(i);
    }
    if (f.recent)
        std::stable_sort(out.begin(), out.end(), [&](size_t a, size_t b)
                         { return recents.indexOf(entries_[a].id) < recents.indexOf(entries_[b].id); });
    return out;
}
bool Library::favourite(const juce::String &id) const
{
    return strings(state_["favourites"]).contains(id);
}
void Library::saveState()
{
    root_.createDirectory();
    root_.getChildFile("state.json").replaceWithText(juce::JSON::toString(state_));
}
void Library::toggleFavourite(const juce::String &id)
{
    auto fav = strings(state_["favourites"]);
    if (fav.contains(id))
        fav.removeString(id);
    else
        fav.add(id);
    state_.getDynamicObject()->setProperty("favourites", array(fav));
    saveState();
}
void Library::used(const juce::String &id)
{
    auto recent = strings(state_["recent"]);
    recent.removeString(id);
    recent.insert(0, id);
    while (recent.size() > 64)
        recent.remove(recent.size() - 1);
    state_.getDynamicObject()->setProperty("recent", array(recent));
    saveState();
}
juce::StringArray Library::collections() const
{
    juce::StringArray result;
    if (auto *o = state_["collections"].getDynamicObject())
        for (auto &p : o->getProperties())
            result.add(p.name.toString());
    return result;
}
juce::Result Library::collect(const juce::String &name, const juce::String &id)
{
    if (name.trim().isEmpty() || name.length() > 128)
        return juce::Result::fail("Collection name is invalid");
    auto collections = state_["collections"];
    if (!collections.isObject())
        collections = object();
    if (collections.getDynamicObject()->getProperties().size() >= 128 &&
        !collections.hasProperty(juce::Identifier(name)))
        return juce::Result::fail("Collection capacity exceeded");
    auto list = strings(collections[juce::Identifier(name)]);
    list.addIfNotAlreadyThere(id);
    collections.getDynamicObject()->setProperty(name, array(list));
    state_.getDynamicObject()->setProperty("collections", collections);
    saveState();
    return juce::Result::ok();
}
juce::Result Library::savePreset(Entry entry)
{
    if (entries_.size() >= 1024)
        return juce::Result::fail("Library capacity exceeded");
    entry.id = "user-" + juce::Uuid().toString();
    entry.kind = "preset";
    entry.pack = "User presets";
    entry.user = true;
    if (entry.license.isEmpty())
        entry.license = "User content";
    if (!valid(entry))
        return juce::Result::fail("Preset parameters are invalid");
    entry.preset.name = entry.name.toStdString();
    auto staged = entries_;
    staged.push_back(entry);
    juce::Array<juce::var> user;
    for (auto &e : staged)
        if (e.user)
            user.add(encode(e));
    if (root_.createDirectory().failed() ||
        !root_.getChildFile("user-library.json").replaceWithText(juce::JSON::toString(juce::var(user))))
        return juce::Result::fail("Could not save preset");
    entries_ = std::move(staged);
    return juce::Result::ok();
}
juce::Result Library::exportState(const juce::File &file) const
{
    return file.replaceWithText(juce::JSON::toString(state_))
               ? juce::Result::ok()
               : juce::Result::fail("Could not export library state");
}
juce::Result Library::importState(const juce::File &file)
{
    if (file.getSize() > 1024 * 1024)
        return juce::Result::fail("Library state too large");
    const auto data = juce::JSON::parse(file);
    if (!data.isObject())
        return juce::Result::fail("Invalid library state");
    auto favouriteIds = strings(state_["favourites"]);
    for (auto &id : strings(data["favourites"]))
        favouriteIds.addIfNotAlreadyThere(id);
    auto recentIds = strings(data["recent"]);
    for (auto &id : strings(state_["recent"]))
        recentIds.addIfNotAlreadyThere(id);
    while (recentIds.size() > 64)
        recentIds.remove(recentIds.size() - 1);
    state_.getDynamicObject()->setProperty("favourites", array(favouriteIds));
    state_.getDynamicObject()->setProperty("recent", array(recentIds));
    auto collections = state_["collections"];
    if (!collections.isObject())
        collections = object();
    if (auto *o = data["collections"].getDynamicObject())
    {
        int n = 0;
        for (auto &p : o->getProperties())
        {
            if (++n > 128)
                break;
            auto ids = strings(collections[p.name]);
            for (auto &id : strings(p.value))
                ids.addIfNotAlreadyThere(id);
            collections.getDynamicObject()->setProperty(p.name, array(ids));
        }
    }
    state_.getDynamicObject()->setProperty("collections", collections);
    saveState();
    return juce::Result::ok();
}
juce::Result Library::materialize(const Entry &entry, juce::File &out, juce::File root)
{
    out = juce::File{};
    if (entry.preset.instrument != InstrumentType::Sampler)
        return juce::Result::ok();
    if (entry.sha256.length() != 64 || !entry.sha256.containsOnly("0123456789abcdef"))
        return juce::Result::fail("Sample checksum is invalid");
    const auto cache =
        (root == juce::File{} ? defaultRoot() : root)
            .getChildFile("audio/" + entry.sha256 +
                          (entry.resource.isNotEmpty() ? juce::String(".wav")
                                                       : juce::File(entry.file).getFileExtension()));
    if (cache.existsAsFile() && juce::SHA256(cache).toHexString() == entry.sha256)
    {
        out = cache;
        return juce::Result::ok();
    }
    if (cache.getParentDirectory().createDirectory().failed())
        return juce::Result::fail("Could not create library cache");
    if (entry.resource.isNotEmpty())
    {
        int size = 0;
        const auto *data = FactoryAssets::getNamedResource(entry.resource.toRawUTF8(), size);
        if (!data || size <= 0 || size > 64 * 1024 * 1024 ||
            juce::SHA256(data, static_cast<size_t>(size)).toHexString() != entry.sha256)
            return juce::Result::fail("Factory sample is missing or corrupt");
        if (!cache.replaceWithData(data, static_cast<size_t>(size)))
            return juce::Result::fail("Could not cache sample");
    }
    else
    {
        const juce::File source(entry.file);
        if (!source.existsAsFile() || source.getSize() > 64 * 1024 * 1024 ||
            juce::SHA256(source).toHexString() != entry.sha256 || !source.copyFileTo(cache))
            return juce::Result::fail("User sample missing or checksum mismatch");
    }
    out = cache;
    return juce::Result::ok();
}
juce::Result Library::importPack(const juce::File &manifest)
{
    if (!manifest.existsAsFile() || manifest.getSize() > 2 * 1024 * 1024)
        return juce::Result::fail("Invalid pack manifest");
    const auto document = juce::JSON::parse(manifest);
    const auto *items = document["entries"].getArray();
    if (static_cast<int>(document["schema"]) != 1 || !items || items->size() > 256)
        return juce::Result::fail("Unsupported pack schema");
    juce::int64 totalBytes = 0;
    for (auto &row : *items)
    {
        const auto file = row["file"].toString();
        if (file.containsAnyOf("/\\") || file.contains(".."))
            return juce::Result::fail("Pack file path is unsafe");
        if (file.isNotEmpty())
        {
            if (manifest.getSiblingFile(file).isSymbolicLink())
                return juce::Result::fail("Pack sample symlinks are not allowed");
            const auto size = manifest.getSiblingFile(file).getSize();
            if (size < 0 || size > 64 * 1024 * 1024)
                return juce::Result::fail("Pack sample too large");
            totalBytes += size;
            if (totalBytes > 512 * 1024 * 1024)
                return juce::Result::fail("Pack is too large");
        }
    }
    std::vector<Entry> staged;
    for (auto &row : *items)
    {
        if (row["tier"].toString().trim().equalsIgnoreCase("premium"))
            return juce::Result::fail("Premium packs require a connected licensing provider");
        auto e = decode(row, true);
        if (!valid(e) || e.license.isEmpty())
            return juce::Result::fail("Invalid pack entry or missing license");
        if (e.preset.instrument == InstrumentType::Sampler)
        {
            if (e.file.isEmpty() || e.file.containsAnyOf("/\\") || e.file.contains(".."))
                return juce::Result::fail("Pack file path is unsafe");
            e.resource.clear();
            e.file = manifest.getSiblingFile(e.file).getFullPathName();
            juce::File cached;
            auto r = materialize(e, cached, root_);
            if (r.failed())
                return r;
            e.file = cached.getFullPathName();
        }
        e.id = "user-" + juce::Uuid().toString();
        staged.push_back(e);
    }
    if (entries_.size() + staged.size() > 1024)
        return juce::Result::fail("Library capacity exceeded");
    entries_.insert(entries_.end(), staged.begin(), staged.end());
    juce::Array<juce::var> user;
    for (auto &e : entries_)
        if (e.user)
            user.add(encode(e));
    root_.createDirectory();
    return root_.getChildFile("user-library.json").replaceWithText(juce::JSON::toString(juce::var(user)))
               ? juce::Result::ok()
               : juce::Result::fail("Could not save imported pack");
}
Idea Library::generate(const juce::String &genre, double bpm, int root, bool minor, const juce::String &mood,
                       int seed) const
{
    Idea idea;
    idea.bpm = juce::jlimit(20.0, 400.0, bpm);
    idea.root = juce::jlimit(36, 84, root);
    idea.minor = minor;
    std::mt19937 random(static_cast<unsigned>(seed));
    auto find = [&](const juce::String &id)
    {
        auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto &e) { return e.id == id; });
        return it == entries_.end() ? Entry{} : *it;
    };
    for (int part = 0; part < 5; ++part)
    {
        IdeaPart p;
        p.entry = find(part == 0   ? "kick"
                       : part == 1 ? "snare"
                       : part == 2 ? "hat"
                       : part == 3 ? "preset-round-sub"
                                   : ((mood == "Dreamy" || genre == "Ambient") ? "pad" : "pluck"));
        if (p.entry.id.isEmpty())
            return {};
        if (part == 0)
        {
            for (int beat = 0; beat < 8; ++beat)
                if (genre == "House" || genre == "Techno" || beat % 2 == 0 || random() % 3 == 0)
                    p.notes.push_back({0, 60, static_cast<double>(beat), .2, .85f});
        }
        else if (part == 1)
        {
            for (int beat = 1; beat < 8; beat += 2)
                p.notes.push_back({0, 60, static_cast<double>(beat), .2, .7f});
        }
        else if (part == 2)
        {
            for (int step = 0; step < 16; ++step)
                if (random() % 5 != 0)
                    p.notes.push_back(
                        {0, 60, step * .5, .12, .35f + static_cast<float>(random() % 30) / 100});
        }
        else
        {
            const int scale[] = {0, minor ? 3 : 4, 5, 7, minor ? 10 : 11};
            for (int step = 0; step < 8; ++step)
            {
                const int pitch = idea.root + (part == 3 ? -12 : 0) + scale[random() % 5];
                p.notes.push_back({0, pitch, static_cast<double>(step), part == 3 ? .75 : .45, .6f});
            }
        }
        if (mood == "Relaxed")
            for (auto &note : p.notes)
                note.velocity *= .7f;
        if (mood == "Dreamy")
            for (auto &note : p.notes)
            {
                note.velocity *= .8f;
                if (part >= 3)
                    note.lengthBeats = std::min(1.5, 8 - note.startBeat);
            }
        idea.parts.push_back(p);
    }
    for (size_t i = 3; i < idea.parts.size(); ++i)
        replace(idea, i, genre, mood, seed + static_cast<int>(i));
    return idea;
}
void Library::replace(Idea &idea, size_t part, const juce::String &genre, const juce::String &mood,
                      int seed) const
{
    if (part >= idea.parts.size())
        return;
    const auto category = idea.parts[part].entry.category;
    std::vector<const Entry *> candidates;
    int best = -1;
    for (auto &e : entries_)
    {
        if (e.category != category || e.kind == "loop" ||
            (part < 3 && e.instrument != idea.parts[part].entry.instrument))
            continue;
        const int score = (e.genre == genre ? 3 : e.genre == "Electronic" ? 1 : 0) + (e.mood == mood ? 2 : 0);
        if (score > best)
        {
            best = score;
            candidates.clear();
        }
        if (score == best)
            candidates.push_back(&e);
    }
    if (candidates.size() > 1)
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                        [&](const auto *e) { return e->id == idea.parts[part].entry.id; }),
                         candidates.end());
    if (!candidates.empty())
        idea.parts[part].entry =
            *candidates[static_cast<size_t>(static_cast<unsigned>(seed)) % candidates.size()];
}
juce::Result Library::applyIdea(Project &project, const Idea &idea, bool arrangement, bool adoptTempo,
                                juce::File cacheRoot)
{
    if (idea.parts.empty() || idea.parts.size() > 8 ||
        project.channels.size() + idea.parts.size() > maxChannels ||
        project.patterns.size() + idea.parts.size() > 128 ||
        (arrangement && project.tracks.size() + idea.parts.size() > 128))
        return juce::Result::fail("Project has no capacity for this idea");
    Project candidate = project;
    if (adoptTempo)
        candidate.tempoMap.points = {{0, idea.bpm}};
    Pattern combined;
    combined.id = PatternID{candidate.nextId()};
    combined.name = "Factory Idea";
    combined.lengthBeats = 8;
    if (!arrangement)
        candidate.patterns.push_back(combined);
    for (auto &part : idea.parts)
    {
        auto channel = part.entry.preset;
        channel.id = ChannelID{candidate.nextId()};
        channel.name = part.entry.name.toStdString();
        juce::File sample;
        auto result = materialize(part.entry, sample, cacheRoot);
        if (result.failed())
            return result;
        if (sample.existsAsFile())
            channel.samplePath = sample.getFullPathName().toStdString();
        const int index = static_cast<int>(candidate.channels.size());
        candidate.channels.push_back(channel);
        auto notes = part.notes;
        for (auto &note : notes)
            note.channel = index;
        if (arrangement)
        {
            Pattern pattern;
            pattern.id = PatternID{candidate.nextId()};
            pattern.name = channel.name + " / Idea";
            pattern.lengthBeats = 8;
            pattern.notes = notes;
            candidate.patterns.push_back(pattern);
            Track track;
            track.id = TrackID{candidate.nextId()};
            track.name = channel.name;
            track.color = channel.color;
            track.clips.push_back({ClipID{candidate.nextId() + 1}, pattern.id, 0, 8, 0, false});
            candidate.tracks.push_back(track);
        }
        else
        {
            auto *p = candidate.findPattern(combined.id);
            p->notes.insert(p->notes.end(), notes.begin(), notes.end());
        }
    }
    if (auto r = candidate.validate(); !r)
        return juce::Result::fail(r.error);
    PlaybackSnapshot compiled;
    if (auto r = compilePlayback(candidate, candidate.patterns.back().id, arrangement, compiled); !r)
        return juce::Result::fail(r.error);
    project = std::move(candidate);
    return juce::Result::ok();
}
bool Library::smokeTest(const juce::File &directory)
{
    const auto testRoot = directory.getChildFile("run-" + juce::Uuid().toString());
    Library library(testRoot);
    if (library.entries().size() < 40)
        return false;
    juce::StringArray categories;
    for (auto &entry : library.entries())
    {
        categories.addIfNotAlreadyThere(entry.category);
        juce::File sample;
        const auto r = materialize(entry, sample, testRoot);
        if (r.failed())
        {
            std::cerr << entry.id << ": " << r.getErrorMessage() << std::endl;
            return false;
        }
        if (entry.preset.instrument == InstrumentType::Sampler &&
            (!sample.existsAsFile() || juce::SHA256(sample).toHexString() != entry.sha256))
            return false;
    }
    for (auto category : {"Drums", "Bass", "Synths", "Keys", "Pads", "FX", "Vocals"})
        if (!categories.contains(category))
            return false;
    Filter filter;
    filter.kind = "loop";
    filter.genre = "House";
    filter.bpm = 120;
    filter.tags = "drums, house";
    if (library.search(filter).size() != 2)
        return false;
    filter = {};
    filter.category = "Bass";
    filter.text = "round";
    if (library.search(filter).empty())
        return false;
    library.toggleFavourite("kick");
    library.used("kick");
    if (library.collect("Test collection", "kick").failed())
        return false;
    Library reopened(testRoot);
    if (!reopened.favourite("kick") || !reopened.collections().contains("Test collection"))
        return false;
    filter = {};
    filter.favourites = true;
    if (reopened.search(filter).size() != 1)
        return false;
    filter = {};
    filter.recent = true;
    if (reopened.search(filter).size() != 1)
        return false;
    const auto exportFile = testRoot.getChildFile("export-state.json");
    if (reopened.exportState(exportFile).failed())
        return false;
    Library transferred(testRoot.getChildFile("transferred"));
    if (transferred.importState(exportFile).failed() || !transferred.favourite("kick"))
        return false;
    auto preset = library.entries().front();
    preset.preset.instrument = InstrumentType::Synth;
    preset.resource.clear();
    preset.file.clear();
    preset.name = "Test user preset";
    preset.preset.cutoff = .22f;
    if (library.savePreset(preset).failed())
        return false;
    Library saved(testRoot);
    filter = {};
    filter.text = "Test user preset";
    if (saved.search(filter).size() != 1)
        return false;
    auto idea = library.generate("House", 120, 60, true, "Energetic", 42),
         repeat = library.generate("House", 120, 60, true, "Energetic", 42);
    if (idea.parts.size() != 5 || repeat.parts.size() != 5)
        return false;
    for (size_t i = 0; i < idea.parts.size(); ++i)
    {
        if (idea.parts[i].notes.empty() || idea.parts[i].notes != repeat.parts[i].notes ||
            idea.parts[i].entry.id != repeat.parts[i].entry.id)
            return false;
        if (i >= 3)
            for (auto &note : idea.parts[i].notes)
            {
                const int pitch = ((note.pitch - 60) % 12 + 12) % 12;
                if (pitch != 0 && pitch != 3 && pitch != 5 && pitch != 7 && pitch != 10)
                    return false;
            }
    }
    auto original = makeDemoProject(), project = original;
    const auto result = applyIdea(project, idea, true, false, testRoot);
    if (result.failed())
    {
        std::cerr << result.getErrorMessage() << std::endl;
        return false;
    }
    if (project.channels.size() != original.channels.size() + 5 || project.tempoMap != original.tempoMap)
        return false;
    for (size_t i = 0; i < original.channels.size(); ++i)
        if (project.channels[i] != original.channels[i])
            return false;
    for (size_t i = 0; i < original.tracks.size(); ++i)
        if (project.tracks[i] != original.tracks[i])
            return false;
    const auto file = testRoot.getChildFile("factory-project.odaw");
    if (!saveProject(project, file.getFullPathName().toStdString()))
        return false;
    Project loaded;
    if (!loadProject(file.getFullPathName().toStdString(), loaded) || loaded != project)
        return false;
    Project patternOnly;
    if (applyIdea(patternOnly, idea, false, true, testRoot).failed() || !patternOnly.tracks.empty() ||
        patternOnly.patterns.size() != 1 || patternOnly.patterns.front().notes.empty())
        return false;
    auto bad = encode(library.entries().front());
    bad.getDynamicObject()->setProperty("file", "../escape.wav");
    auto manifest = object();
    manifest.getDynamicObject()->setProperty("schema", 1);
    manifest.getDynamicObject()->setProperty("entries", juce::var(juce::Array<juce::var>{bad}));
    const auto badFile = testRoot.getChildFile("unsafe-pack.json");
    badFile.replaceWithText(juce::JSON::toString(manifest));
    const auto count = library.entries().size();
    if (library.importPack(badFile).wasOk() || library.entries().size() != count)
        return false;
    bad.getDynamicObject()->setProperty("file", "safe-pack-sample.wav");
    bad.getDynamicObject()->setProperty("tier", "premium");
    badFile.replaceWithText(juce::JSON::toString(manifest));
    const auto premium = library.importPack(badFile);
    if (premium.wasOk() || !premium.getErrorMessage().contains("Premium"))
        return false;
    bad.getDynamicObject()->setProperty("tier", "free");
    juce::File source;
    if (materialize(library.entries().front(), source, testRoot).failed() ||
        !source.copyFileTo(testRoot.getChildFile("safe-pack-sample.wav")))
        return false;
    badFile.replaceWithText(juce::JSON::toString(manifest));
    if (library.importPack(badFile).failed() || library.entries().size() != count + 1)
        return false;
    transferred.toggleFavourite("snare");
    if (transferred.importState(exportFile).failed() || !transferred.favourite("snare") ||
        !transferred.favourite("kick"))
        return false;
    std::cout << "Factory asset checksums / metadata filters / favourites / collections / preset persistence "
                 "/ deterministic ideas / existing-project preservation / pack safety passed"
              << std::endl;
    return true;
}
} // namespace forge::library
