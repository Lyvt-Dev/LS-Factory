#include "Delay.h"
#include <algorithm>
namespace forge
{
void Delay::prepare(double rate)
{
    data_.assign(static_cast<std::size_t>(rate * 0.375), 0);
    index_ = 0;
}
void Delay::reset() noexcept
{
    std::fill(data_.begin(), data_.end(), 0);
    index_ = 0;
}
float Delay::process(float input, float wet) noexcept
{
    if (data_.empty())
        return input;
    const float echo = data_[index_];
    data_[index_] = input + echo * 0.32f;
    index_ = (index_ + 1) % data_.size();
    return input + echo * std::clamp(wet, 0.0f, 1.0f);
}
} // namespace forge
