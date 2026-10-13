#include "Sampler.h"
namespace forge
{
float SampleData::read(double position) const noexcept
{
    if (position < 0 || mono.size() < 2 || position >= static_cast<double>(mono.size() - 1))
        return 0;
    const auto i = static_cast<std::size_t>(position);
    const auto fraction = static_cast<float>(position - static_cast<double>(i));
    return mono[i] + fraction * (mono[i + 1] - mono[i]);
}
} // namespace forge
