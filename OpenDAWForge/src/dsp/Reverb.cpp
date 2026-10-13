#include "Reverb.h"
#include <algorithm>
namespace forge
{
void Reverb::prepare(double rate)
{
    const double times[] = {0.0297, 0.0371, 0.0411, 0.0437};
    for (std::size_t i = 0; i < 4; ++i)
        combs_[i].data.assign(std::max<std::size_t>(1, static_cast<std::size_t>(rate * times[i])), 0);
    allpass_[0].data.assign(std::max<std::size_t>(1, static_cast<std::size_t>(rate * 0.005)), 0);
    allpass_[1].data.assign(std::max<std::size_t>(1, static_cast<std::size_t>(rate * 0.0017)), 0);
    reset();
}
void Reverb::reset() noexcept
{
    for (auto *collection : {&combs_})
        for (auto &c : *collection)
        {
            std::fill(c.data.begin(), c.data.end(), 0);
            c.position = 0;
            c.damp = 0;
        }
    for (auto &c : allpass_)
    {
        std::fill(c.data.begin(), c.data.end(), 0);
        c.position = 0;
        c.damp = 0;
    }
}
float Reverb::process(float input, float wet) noexcept
{
    if (wet <= 0)
        return input;
    float output = 0;
    for (auto &c : combs_)
    {
        if (c.data.empty())
            continue;
        const float tap = c.data[c.position];
        c.damp += 0.25f * (tap - c.damp);
        c.data[c.position] = input * 0.2f + c.damp * 0.78f;
        c.position = (c.position + 1) % c.data.size();
        output += tap * 0.25f;
    }
    for (auto &a : allpass_)
    {
        if (a.data.empty())
            continue;
        const float delayed = a.data[a.position];
        const float next = output + delayed * 0.5f;
        a.data[a.position] = next;
        output = delayed - next * 0.5f;
        a.position = (a.position + 1) % a.data.size();
    }
    return input + output * std::clamp(wet, 0.0f, 1.0f);
}
} // namespace forge
