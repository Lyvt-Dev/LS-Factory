#include "EQ.h"
#include "core/Constants.h"
#include <algorithm>
#include <cmath>
namespace forge
{
void EQ::prepare(double rate) noexcept
{
    rate_ = rate;
    const double frequencies[] = {60, 120, 250, 500, 1000, 2500, 6000, 12000};
    for (std::size_t i = 0; i < 8; ++i)
        setBand(i, frequencies[i], 0, 0.707);
    reset();
}
void EQ::reset() noexcept
{
    for (auto &b : bands_)
    {
        b.z1 = 0;
        b.z2 = 0;
    }
}
void EQ::setBand(std::size_t i, double frequency, double gain, double q) noexcept
{
    if (i >= bands_.size())
        return;
    frequency = std::clamp(frequency, 10.0, rate_ * 0.45);
    gain = std::clamp(gain, -24.0, 24.0);
    q = std::clamp(q, 0.1, 20.0);
    const double a = std::pow(10.0, gain / 40), omega = 2 * pi * frequency / rate_,
                 alpha = std::sin(omega) / (2 * q), cosine = std::cos(omega), a0 = 1 + alpha / a;
    auto &b = bands_[i];
    b.b0 = (1 + alpha * a) / a0;
    b.b1 = -2 * cosine / a0;
    b.b2 = (1 - alpha * a) / a0;
    b.a1 = -2 * cosine / a0;
    b.a2 = (1 - alpha / a) / a0;
}
float EQ::process(float input) noexcept
{
    double x = input;
    for (auto &b : bands_)
    {
        const double out = b.b0 * x + b.z1;
        b.z1 = b.b1 * x - b.a1 * out + b.z2;
        b.z2 = b.b2 * x - b.a2 * out;
        x = out;
    }
    return static_cast<float>(x);
}
} // namespace forge
