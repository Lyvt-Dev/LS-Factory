#include "BinaryFormat.h"
#include <bit>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#include <zlib.h>
namespace forge
{
namespace
{
constexpr std::uint32_t formatVersion = 4;
constexpr std::size_t maxFileBytes = 16 * 1024 * 1024;
struct Writer
{
    std::vector<std::uint8_t> bytes;
    void u64(std::uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            bytes.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    void real(double v)
    {
        u64(std::bit_cast<std::uint64_t>(v));
    }
    void text(const std::string &s)
    {
        u64(s.size());
        bytes.insert(bytes.end(), s.begin(), s.end());
    }
};
struct Reader
{
    std::span<const std::uint8_t> bytes;
    std::size_t offset = 0;
    std::uint64_t u64()
    {
        if (bytes.size() - offset < 8)
            throw std::runtime_error("Truncated project");
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(bytes[offset++]) << (i * 8);
        return v;
    }
    double real()
    {
        return std::bit_cast<double>(u64());
    }
    std::size_t count(std::size_t max)
    {
        auto n = u64();
        if (n > max)
            throw std::runtime_error("Project count exceeds limit");
        return static_cast<std::size_t>(n);
    }
    std::string text(std::size_t max = 4096)
    {
        auto n = count(max);
        if (n > bytes.size() - offset)
            throw std::runtime_error("Truncated string");
        std::string s(reinterpret_cast<const char *>(bytes.data() + offset), n);
        offset += n;
        return s;
    }
    bool boolean()
    {
        auto n = u64();
        if (n > 1)
            throw std::runtime_error("Invalid boolean");
        return n != 0;
    }
    int integer(int max)
    {
        auto n = u64();
        if (n > static_cast<std::uint64_t>(max))
            throw std::runtime_error("Invalid integer");
        return static_cast<int>(n);
    }
};
std::uint64_t hash(std::span<const std::uint8_t> bytes)
{
    std::uint64_t h = 14695981039346656037ull;
    for (auto b : bytes)
    {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}
} // namespace
Result saveProject(const Project &p, const std::filesystem::path &path)
{
    if (auto r = p.validate(); !r)
        return r;
    try
    {
        Writer w;
        w.text(p.metadata.name);
        w.text(p.metadata.author);
        w.text(p.metadata.comments);
        w.real(p.loopStart);
        w.real(p.loopEnd);
        w.u64(p.loopEnabled);
        w.real(p.masterVolume);
        w.u64(p.tempoMap.points.size());
        for (auto t : p.tempoMap.points)
        {
            w.real(t.beat);
            w.real(t.bpm);
        }
        w.u64(p.channels.size());
        for (const auto &c : p.channels)
        {
            w.u64(c.id.value());
            w.text(c.name);
            w.u64(c.color);
            w.u64(static_cast<unsigned>(c.instrument));
            w.real(c.volume);
            w.real(c.pan);
            w.real(c.cutoff);
            w.real(c.delay);
            w.u64(c.muted);
            w.u64(c.soloed);
            w.text(c.samplePath);
            w.u64(c.sampleReversed);
            w.real(c.reverb);
            w.real(c.compression);
            w.real(c.drive);
        }
        w.u64(p.patterns.size());
        for (const auto &pat : p.patterns)
        {
            w.u64(pat.id.value());
            w.text(pat.name);
            w.u64(pat.color);
            w.real(pat.lengthBeats);
            w.u64(pat.notes.size());
            for (auto n : pat.notes)
            {
                w.u64(n.channel);
                w.u64(n.pitch);
                w.real(n.startBeat);
                w.real(n.lengthBeats);
                w.real(n.velocity);
            }
        }
        w.u64(p.tracks.size());
        for (const auto &t : p.tracks)
        {
            w.u64(t.id.value());
            w.text(t.name);
            w.u64(t.color);
            w.u64(t.clips.size());
            for (const auto &c : t.clips)
            {
                w.u64(c.id.value());
                w.u64(c.patternId.value());
                w.real(c.startBeat);
                w.real(c.lengthBeats);
                w.u64(static_cast<std::uint64_t>(c.transpositionSemitones + 127));
                w.u64(c.muted);
            }
        }
        w.u64(p.automation.size());
        for (const auto &a : p.automation)
        {
            w.u64(a.id.value());
            w.text(a.name);
            w.u64(a.channel);
            w.u64(static_cast<unsigned>(a.target));
            w.u64(static_cast<unsigned>(a.interpolation));
            w.u64(a.points.size());
            for (auto point : a.points)
            {
                w.real(point.beat);
                w.real(point.value);
            }
        }
        w.u64(p.masterInserts.size());
        for (const auto &slot : p.masterInserts)
        {
            w.u64(slot.id.value());
            w.text(slot.name);
            w.text(slot.descriptionXml);
            w.u64(slot.bypassed);
            w.u64(slot.state.size());
            w.bytes.insert(w.bytes.end(), slot.state.begin(), slot.state.end());
            w.u64(slot.normalizedParameters.size());
            for (float parameter : slot.normalizedParameters)
                w.real(parameter);
        }
        if (w.bytes.size() > maxFileBytes)
            return Result::failure("Project exceeds file limit");
        Writer packed;
        packed.u64(w.bytes.size());
        uLongf compressedSize = compressBound(static_cast<uLong>(w.bytes.size()));
        const auto begin = packed.bytes.size();
        packed.bytes.resize(begin + compressedSize);
        if (compress2(packed.bytes.data() + begin, &compressedSize, w.bytes.data(),
                      static_cast<uLong>(w.bytes.size()), Z_BEST_SPEED) != Z_OK)
            throw std::runtime_error("Project compression failed");
        packed.bytes.resize(begin + compressedSize);
        if (packed.bytes.size() > maxFileBytes)
            throw std::runtime_error("Compressed project exceeds file limit");
        Writer header;
        header.u64(formatVersion);
        header.u64(packed.bytes.size());
        header.u64(hash(packed.bytes));
        auto temp = path;
        temp += ".tmp";
        {
            std::ofstream f(temp, std::ios::binary | std::ios::trunc);
            f.write("LSFODAW1", 8);
            f.write(reinterpret_cast<const char *>(header.bytes.data()),
                    static_cast<std::streamsize>(header.bytes.size()));
            f.write(reinterpret_cast<const char *>(packed.bytes.data()),
                    static_cast<std::streamsize>(packed.bytes.size()));
            f.flush();
            if (!f)
                throw std::runtime_error("Cannot write project");
        }
        // Keep a recoverable prior version on Windows where rename cannot replace.
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        if (ec)
        {
            auto backup = path;
            backup += ".bak";
            if (std::filesystem::exists(path))
            {
                std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(path);
            }
            std::filesystem::rename(temp, path);
        }
        return {};
    }
    catch (const std::exception &e)
    {
        return Result::failure(e.what());
    }
}
Result loadProject(const std::filesystem::path &path, Project &output)
{
    try
    {
        const auto size = std::filesystem::file_size(path);
        if (size < 32 || size > maxFileBytes + 32)
            throw std::runtime_error("Invalid project size");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
        if (!f)
            throw std::runtime_error("Cannot read project");
        if (std::memcmp(bytes.data(), "LSFODAW1", 8) != 0)
            throw std::runtime_error("Not an LS Factory project");
        Reader header{std::span(bytes).subspan(8, 24)};
        const auto version = header.u64();
        if (version != 3 && version != formatVersion)
            throw std::runtime_error("Unsupported project version");
        if (header.u64() != size - 32)
            throw std::runtime_error("Invalid payload size");
        if (header.u64() != hash(std::span(bytes).subspan(32)))
            throw std::runtime_error("Project checksum mismatch");
        Reader packed{std::span(bytes).subspan(32)};
        const auto rawSize = packed.count(maxFileBytes);
        std::vector<std::uint8_t> raw(rawSize);
        uLongf decoded = static_cast<uLongf>(rawSize);
        uLong source = static_cast<uLong>(packed.bytes.size() - 8);
        if (uncompress2(raw.data(), &decoded, packed.bytes.data() + 8, &source) != Z_OK ||
            decoded != rawSize || source != packed.bytes.size() - 8)
            throw std::runtime_error("Invalid compressed project payload");
        Reader r{raw};
        Project p;
        p.metadata = {r.text(), r.text(), r.text()};
        p.loopStart = r.real();
        p.loopEnd = r.real();
        p.loopEnabled = r.boolean();
        p.masterVolume = static_cast<float>(r.real());
        auto count = r.count(1024);
        p.tempoMap.points.clear();
        for (std::size_t i = 0; i < count; ++i)
            p.tempoMap.points.push_back({r.real(), r.real()});
        count = r.count(64);
        for (std::size_t i = 0; i < count; ++i)
        {
            Channel c;
            c.id = ChannelID{r.u64()};
            c.name = r.text();
            c.color = static_cast<std::uint32_t>(r.u64());
            c.instrument = static_cast<InstrumentType>(r.integer(4));
            c.volume = static_cast<float>(r.real());
            c.pan = static_cast<float>(r.real());
            c.cutoff = static_cast<float>(r.real());
            c.delay = static_cast<float>(r.real());
            c.muted = r.boolean();
            c.soloed = r.boolean();
            c.samplePath = r.text();
            c.sampleReversed = r.boolean();
            c.reverb = static_cast<float>(r.real());
            c.compression = static_cast<float>(r.real());
            c.drive = static_cast<float>(r.real());
            p.channels.push_back(c);
        }
        count = r.count(128);
        for (std::size_t i = 0; i < count; ++i)
        {
            Pattern pat;
            pat.id = PatternID{r.u64()};
            pat.name = r.text();
            pat.color = static_cast<std::uint32_t>(r.u64());
            pat.lengthBeats = r.real();
            auto n = r.count(1024);
            for (std::size_t j = 0; j < n; ++j)
                pat.notes.push_back(
                    {r.integer(63), r.integer(127), r.real(), r.real(), static_cast<float>(r.real())});
            p.patterns.push_back(pat);
        }
        count = r.count(128);
        for (std::size_t i = 0; i < count; ++i)
        {
            Track t;
            t.id = TrackID{r.u64()};
            t.name = r.text();
            t.color = static_cast<std::uint32_t>(r.u64());
            auto n = r.count(1024);
            for (std::size_t j = 0; j < n; ++j)
            {
                PatternClip c;
                c.id = ClipID{r.u64()};
                c.patternId = PatternID{r.u64()};
                c.startBeat = r.real();
                c.lengthBeats = r.real();
                c.transpositionSemitones = r.integer(254) - 127;
                c.muted = r.boolean();
                t.clips.push_back(c);
            }
            p.tracks.push_back(t);
        }
        count = r.count(16);
        for (std::size_t i = 0; i < count; ++i)
        {
            AutomationLane a;
            a.id = TrackID{r.u64()};
            a.name = r.text();
            a.channel = r.integer(63);
            a.target = static_cast<AutomationTarget>(r.integer(4));
            a.interpolation = static_cast<Interpolation>(r.integer(2));
            auto n = r.count(64);
            for (std::size_t j = 0; j < n; ++j)
                a.points.push_back({r.real(), static_cast<float>(r.real())});
            p.automation.push_back(a);
        }
        if (version >= 4)
        {
            count = r.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                PluginSlot slot;
                slot.id = PluginInstanceID{r.u64()};
                slot.name = r.text();
                slot.descriptionXml = r.text(65536);
                slot.bypassed = r.boolean();
                const auto bytes = r.count(4 * 1024 * 1024);
                if (bytes > r.bytes.size() - r.offset)
                    throw std::runtime_error("Truncated plugin state");
                slot.state.assign(r.bytes.begin() + static_cast<std::ptrdiff_t>(r.offset),
                                  r.bytes.begin() + static_cast<std::ptrdiff_t>(r.offset + bytes));
                r.offset += bytes;
                const auto parameters = r.count(10000);
                for (std::size_t j = 0; j < parameters; ++j)
                    slot.normalizedParameters.push_back(static_cast<float>(r.real()));
                p.masterInserts.push_back(std::move(slot));
            }
        }
        if (r.offset != r.bytes.size())
            throw std::runtime_error("Unexpected trailing data");
        if (auto result = p.validate(); !result)
            return result;
        output = std::move(p);
        return {};
    }
    catch (const std::exception &e)
    {
        return Result::failure(e.what());
    }
}
} // namespace forge
