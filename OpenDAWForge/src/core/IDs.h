#pragma once
#include <compare>
#include <cstdint>
namespace forge
{
template <class Tag> class StrongId
{
  public:
    explicit constexpr StrongId(std::uint64_t value = 0) noexcept : value_(value) {}
    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return value_;
    }
    auto operator<=>(const StrongId &) const = default;

  private:
    std::uint64_t value_;
};
struct ChannelTag
{
};
struct PatternTag
{
};
struct ClipTag
{
};
struct TrackTag
{
};
struct MixerChannelTag
{
};
struct PluginInstanceTag
{
};
struct AudioSourceTag
{
};
using ChannelID = StrongId<ChannelTag>;
using PatternID = StrongId<PatternTag>;
using ClipID = StrongId<ClipTag>;
using TrackID = StrongId<TrackTag>;
using MixerChannelID = StrongId<MixerChannelTag>;
using PluginInstanceID = StrongId<PluginInstanceTag>;
using AudioSourceID = StrongId<AudioSourceTag>;
} // namespace forge
