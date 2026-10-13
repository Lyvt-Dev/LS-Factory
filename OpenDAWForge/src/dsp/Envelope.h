#pragma once
namespace forge
{
class Envelope
{
  public:
    void prepare(double rate) noexcept
    {
        rate_ = rate;
    }
    void set(float attack, float decay, float sustain, float release) noexcept;
    void on() noexcept
    {
        value_ = 0;
        state_ = State::Attack;
    }
    void off() noexcept
    {
        releaseStep_ = value_ / static_cast<float>(rate_ * release_);
        state_ = State::Release;
    }
    void reset() noexcept
    {
        value_ = 0;
        state_ = State::Idle;
    }
    float next() noexcept;
    bool active() const noexcept
    {
        return state_ != State::Idle;
    }

  private:
    enum class State
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    State state_ = State::Idle;
    double rate_ = 48000;
    float attack_ = 0.005f, decay_ = 0.14f, sustain_ = 0.6f, release_ = 0.18f, value_ = 0, releaseStep_ = 0;
};
} // namespace forge
