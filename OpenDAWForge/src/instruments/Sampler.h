#pragma once
#include <cstddef>
#include <vector>
namespace forge
{
struct SampleData
{
    std::vector<float> mono;
    double sampleRate = 48000;
    int rootPitch = 60;
    [[nodiscard]] float read(double position) const noexcept;
};
} // namespace forge
