#pragma once
#include <cmath>
namespace forge
{
[[nodiscard]] inline double midiFrequency(int pitch) noexcept
{
    return 440.0 * std::exp2((pitch - 69) / 12.0);
}
[[nodiscard]] inline float dbToGain(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}
} // namespace forge
