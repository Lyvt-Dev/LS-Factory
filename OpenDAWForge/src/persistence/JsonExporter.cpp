#include "JsonExporter.h"
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
namespace forge
{
namespace
{
std::string quote(const std::string &text)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text)
    {
        switch (c)
        {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 32)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c)
                    << std::dec;
            else
                out << c;
        }
    }
    out << '"';
    return out.str();
}
} // namespace
std::string projectJson(const Project &p)
{
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << std::setprecision(17) << std::boolalpha;
    o << "{\n  \"format\": \"LSFactory-debug\", \"version\": 4,\n  \"metadata\": {\"name\": "
      << quote(p.metadata.name) << ", \"author\": " << quote(p.metadata.author)
      << ", \"comments\": " << quote(p.metadata.comments) << "},\n  \"loop\": {\"start\": " << p.loopStart
      << ", \"end\": " << p.loopEnd << ", \"enabled\": " << p.loopEnabled
      << "},\n  \"masterVolume\": " << p.masterVolume << ",\n  \"tempoMap\": [";
    for (std::size_t i = 0; i < p.tempoMap.points.size(); ++i)
    {
        auto t = p.tempoMap.points[i];
        if (i)
            o << ',';
        o << "{\"beat\":" << t.beat << ",\"bpm\":" << t.bpm << '}';
    }
    o << "],\n  \"channels\": [";
    for (std::size_t i = 0; i < p.channels.size(); ++i)
    {
        const auto &c = p.channels[i];
        if (i)
            o << ',';
        o << "\n    {\"id\":" << c.id.value() << ",\"name\":" << quote(c.name) << ",\"color\":" << c.color
          << ",\"instrument\":" << static_cast<int>(c.instrument) << ",\"volume\":" << c.volume
          << ",\"pan\":" << c.pan << ",\"cutoff\":" << c.cutoff << ",\"delay\":" << c.delay
          << ",\"muted\":" << c.muted << ",\"soloed\":" << c.soloed
          << ",\"samplePath\":" << quote(c.samplePath) << ",\"sampleReversed\":" << c.sampleReversed
          << ",\"reverb\":" << c.reverb << ",\"compression\":" << c.compression << ",\"drive\":" << c.drive
          << '}';
    }
    o << "\n  ],\n  \"patterns\": [";
    for (std::size_t i = 0; i < p.patterns.size(); ++i)
    {
        const auto &pat = p.patterns[i];
        if (i)
            o << ',';
        o << "\n    {\"id\":" << pat.id.value() << ",\"name\":" << quote(pat.name)
          << ",\"color\":" << pat.color << ",\"length\":" << pat.lengthBeats << ",\"notes\":[";
        for (std::size_t j = 0; j < pat.notes.size(); ++j)
        {
            auto n = pat.notes[j];
            if (j)
                o << ',';
            o << "{\"channel\":" << n.channel << ",\"pitch\":" << n.pitch << ",\"start\":" << n.startBeat
              << ",\"length\":" << n.lengthBeats << ",\"velocity\":" << n.velocity << '}';
        }
        o << "]}";
    }
    o << "\n  ],\n  \"tracks\": [";
    for (std::size_t i = 0; i < p.tracks.size(); ++i)
    {
        const auto &t = p.tracks[i];
        if (i)
            o << ',';
        o << "\n    {\"id\":" << t.id.value() << ",\"name\":" << quote(t.name) << ",\"color\":" << t.color
          << ",\"clips\":[";
        for (std::size_t j = 0; j < t.clips.size(); ++j)
        {
            const auto &c = t.clips[j];
            if (j)
                o << ',';
            o << "{\"id\":" << c.id.value() << ",\"pattern\":" << c.patternId.value()
              << ",\"start\":" << c.startBeat << ",\"length\":" << c.lengthBeats
              << ",\"transpose\":" << c.transpositionSemitones << ",\"muted\":" << c.muted << '}';
        }
        o << "]}";
    }
    o << "\n  ],\n  \"automation\": [";
    for (std::size_t i = 0; i < p.automation.size(); ++i)
    {
        const auto &a = p.automation[i];
        if (i)
            o << ',';
        o << "{\"id\":" << a.id.value() << ",\"name\":" << quote(a.name) << ",\"channel\":" << a.channel
          << ",\"target\":" << static_cast<int>(a.target)
          << ",\"interpolation\":" << static_cast<int>(a.interpolation) << ",\"points\":[";
        for (std::size_t j = 0; j < a.points.size(); ++j)
        {
            if (j)
                o << ',';
            o << "{\"beat\":" << a.points[j].beat << ",\"value\":" << a.points[j].value << '}';
        }
        o << "]}";
    }
    o << "],\n  \"masterInserts\": [";
    const char *digits = "0123456789abcdef";
    for (std::size_t i = 0; i < p.masterInserts.size(); ++i)
    {
        const auto &slot = p.masterInserts[i];
        if (i)
            o << ',';
        std::string state;
        state.reserve(slot.state.size() * 2);
        for (auto byte : slot.state)
        {
            state += digits[byte >> 4];
            state += digits[byte & 15];
        }
        o << "{\"id\":" << slot.id.value() << ",\"name\":" << quote(slot.name)
          << ",\"descriptionXml\":" << quote(slot.descriptionXml) << ",\"bypassed\":" << slot.bypassed
          << ",\"stateHex\":" << quote(state) << ",\"normalizedParameters\":[";
        for (std::size_t j = 0; j < slot.normalizedParameters.size(); ++j)
        {
            if (j)
                o << ',';
            o << slot.normalizedParameters[j];
        }
        o << "]}";
    }
    o << "]\n}\n";
    return o.str();
}
Result exportJson(const Project &p, const std::filesystem::path &path)
{
    if (auto r = p.validate(); !r)
        return r;
    std::ofstream file(path);
    file << projectJson(p);
    return file ? Result{} : Result::failure("Cannot write JSON export");
}
} // namespace forge
