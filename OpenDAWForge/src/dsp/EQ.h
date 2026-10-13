#pragma once
#include <array>
namespace forge
{
class EQ
{
  public:
    void prepare(double rate) noexcept;
    void setBand(std::size_t band, double frequency, double gainDb, double q) noexcept;
    void reset() noexcept;
    float process(float input) noexcept;

  private:
    struct Band
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    };
    std::array<Band, 8> bands_;
    double rate_ = 48000;
};
} // namespace forge
