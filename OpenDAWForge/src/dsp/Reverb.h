#pragma once
#include <array>
#include <vector>
namespace forge
{
class Reverb
{
  public:
    void prepare(double rate);
    void reset() noexcept;
    float process(float input, float wet) noexcept;

  private:
    struct Comb
    {
        std::vector<float> data;
        std::size_t position = 0;
        float damp = 0;
    };
    std::array<Comb, 4> combs_;
    std::array<Comb, 2> allpass_;
};
} // namespace forge
