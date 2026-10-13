#pragma once
#include <cstdint>
namespace forge
{
[[nodiscard]] double beatsToSamples(double beats, double bpm, double sampleRate) noexcept;
[[nodiscard]] double samplesToBeats(std::int64_t samples, double bpm, double sampleRate) noexcept;
} // namespace forge
