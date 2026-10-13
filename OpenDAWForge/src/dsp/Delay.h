#pragma once
#include <cstddef>
#include <vector>
namespace forge
{
class Delay
{
  public:
    void prepare(double rate);
    void reset() noexcept;
    float process(float input, float wet) noexcept;

  private:
    std::vector<float> data_;
    std::size_t index_ = 0;
};
} // namespace forge
