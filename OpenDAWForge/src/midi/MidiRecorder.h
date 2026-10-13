#pragma once
#include "model/Project.h"
#include <array>
#include <optional>
namespace forge
{
struct CapturedMidi
{
    int channel = 0, pitch = 60;
    float velocity = 0;
    bool on = false;
    double beat = 0;
};
class MidiRecorder
{
  public:
    void begin(double length, int targetChannel)
    {
        length_ = length;
        target_ = targetChannel;
        notes_.clear();
        for (auto &n : held_)
            n.reset();
    }
    void receive(const CapturedMidi &);
    std::vector<MidiNote> finish(double beat);

  private:
    std::array<std::optional<CapturedMidi>, 128> held_{};
    std::vector<MidiNote> notes_;
    double length_ = 4;
    int target_ = 0;
};
} // namespace forge
