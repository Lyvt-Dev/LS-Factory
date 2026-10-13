#pragma once
#include <cstdint>
namespace forge
{
enum class Wave
{
    Sine,
    Saw,
    Square,
    Triangle,
    Noise
};
class Oscillator
{
  public:
    void reset() noexcept
    {
        phase_ = 0;
    }
    float next(double frequency, double rate, Wave wave) noexcept;

  private:
    double phase_ = 0;
    std::uint32_t noise_ = 0x96b8d3a1;
};
} // namespace forge
