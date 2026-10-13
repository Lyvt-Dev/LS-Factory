#pragma once
namespace forge
{
class Compressor
{
  public:
    void prepare(double rate) noexcept;
    void reset() noexcept
    {
        envelope_ = 0;
        gain_ = 1;
    }
    float process(float input, float sidechain, float thresholdDb, float ratio = 4) noexcept;
    float gainReduction() const noexcept
    {
        return gain_;
    }

  private:
    float attack_ = 0, release_ = 0, envelope_ = 0, gain_ = 1;
};
} // namespace forge
