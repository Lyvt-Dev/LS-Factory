#include "Envelope.h"
#include <algorithm>
namespace forge
{
void Envelope::set(float a, float d, float s, float r) noexcept
{
    attack_ = std::max(a, 0.0001f);
    decay_ = std::max(d, 0.0001f);
    sustain_ = std::clamp(s, 0.0f, 1.0f);
    release_ = std::max(r, 0.0001f);
}
float Envelope::next() noexcept
{
    switch (state_)
    {
    case State::Idle:
        return 0;
    case State::Attack:
        value_ += 1.0f / static_cast<float>(rate_ * attack_);
        if (value_ >= 1)
        {
            value_ = 1;
            state_ = State::Decay;
        }
        break;
    case State::Decay:
        value_ -= (1 - sustain_) / static_cast<float>(rate_ * decay_);
        if (value_ <= sustain_)
        {
            value_ = sustain_;
            state_ = sustain_ == 0 ? State::Idle : State::Sustain;
        }
        break;
    case State::Sustain:
        break;
    case State::Release:
        value_ -= releaseStep_;
        if (value_ <= 0)
        {
            value_ = 0;
            state_ = State::Idle;
        }
        break;
    }
    return value_;
}
} // namespace forge
