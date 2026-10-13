#pragma once
#include <cstddef>
namespace forge
{
inline constexpr std::size_t maxChannels = 64;
inline constexpr std::size_t maxNotes = 1024;
inline constexpr std::size_t maxVoices = 64;
inline constexpr std::size_t maxEvents = 2048;
inline constexpr double pi = 3.14159265358979323846;
inline constexpr double defaultSampleRate = 48000.0;
} // namespace forge
