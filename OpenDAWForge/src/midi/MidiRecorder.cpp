#include "MidiRecorder.h"
#include <algorithm>
#include <cmath>
namespace forge
{
void MidiRecorder::receive(const CapturedMidi &event)
{
    if (event.pitch < 0 || event.pitch > 127 || event.channel != target_)
        return;
    auto &held = held_[static_cast<std::size_t>(event.pitch)];
    if (event.on)
    {
        held = event;
        return;
    }
    if (!held)
        return;
    auto start = std::fmod(std::max(0.0, held->beat), length_);
    double duration = event.beat - held->beat;
    if (duration <= 0)
        duration += length_;
    notes_.push_back({target_, event.pitch, start, std::clamp(duration, 0.0625, length_), held->velocity});
    held.reset();
}
std::vector<MidiNote> MidiRecorder::finish(double beat)
{
    for (int i = 0; i < 128; ++i)
        if (held_[static_cast<std::size_t>(i)])
            receive({target_, i, 0, false, beat});
    return std::move(notes_);
}
} // namespace forge
