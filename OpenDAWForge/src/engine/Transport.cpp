#include "Transport.h"
#include <algorithm>
#include <cmath>
namespace forge
{
void Transport::setTempo(double bpm) noexcept
{
    if (std::isfinite(bpm))
        bpm_ = std::clamp(bpm, 20.0, 400.0);
}
} // namespace forge
