#include "Compressor.h"
#include <algorithm>
#include <cmath>
namespace forge
{
void Compressor::prepare(double rate) noexcept
{
    attack_ = static_cast<float>(std::exp(-1 / (rate * 0.01)));
    release_ = static_cast<float>(std::exp(-1 / (rate * 0.1)));
    reset();
}
float Compressor::process(float input, float sidechain, float threshold, float ratio) noexcept
{
    const float magnitude = std::abs(sidechain);
    const float coefficient = magnitude > envelope_ ? attack_ : release_;
    envelope_ = coefficient * envelope_ + (1 - coefficient) * magnitude;
    const float level = 20 * std::log10(std::max(envelope_, 1e-8f));
    const float over = std::max(0.0f, level - threshold);
    gain_ = std::pow(10.0f, -over * (1 - 1 / std::max(1.0f, ratio)) / 20);
    return input * gain_;
}
} // namespace forge
