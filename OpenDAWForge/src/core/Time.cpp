#include "Time.h"
namespace forge
{
double beatsToSamples(double beats, double bpm, double rate) noexcept
{
    return bpm > 0 && rate > 0 ? beats * rate * 60.0 / bpm : 0.0;
}
double samplesToBeats(std::int64_t samples, double bpm, double rate) noexcept
{
    return rate > 0 && bpm > 0 ? static_cast<double>(samples) * bpm / (60.0 * rate) : 0.0;
}
} // namespace forge
