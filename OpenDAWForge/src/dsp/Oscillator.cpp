#include "Oscillator.h"
#include "core/Constants.h"
#include <algorithm>
#include <cmath>
namespace forge
{
namespace
{
double blep(double t, double dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1;
    }
    if (t > 1 - dt)
    {
        t = (t - 1) / dt;
        return t * t + t + t + 1;
    }
    return 0;
}
} // namespace
float Oscillator::next(double frequency, double rate, Wave wave) noexcept
{
    const double dt = std::clamp(frequency / rate, 0.000001, 0.49);
    double value = 0;
    switch (wave)
    {
    case Wave::Sine:
        value = std::sin(2 * pi * phase_);
        break;
    case Wave::Saw:
        value = 2 * phase_ - 1 - blep(phase_, dt);
        break;
    case Wave::Square:
        value = (phase_ < 0.5 ? 1 : -1) + blep(phase_, dt) - blep(std::fmod(phase_ + 0.5, 1.0), dt);
        break;
    case Wave::Triangle:
        value = 1 - 4 * std::abs(phase_ - 0.5);
        break;
    case Wave::Noise:
        noise_ ^= noise_ << 13;
        noise_ ^= noise_ >> 17;
        noise_ ^= noise_ << 5;
        value = static_cast<double>(noise_) / 2147483648.0 - 1;
        break;
    }
    phase_ += dt;
    if (phase_ >= 1)
        phase_ -= 1;
    return static_cast<float>(value);
}
} // namespace forge
