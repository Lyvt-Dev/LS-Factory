#pragma once
#include <juce_cryptography/juce_cryptography.h>
namespace forge::installer
{
struct Options
{
    juce::File home, destination, payloadDirectory;
    bool desktopShortcut = false;
};
struct Outcome
{
    juce::Result result;
    juce::File launcher;
};
[[nodiscard]] Outcome install(const Options &);
[[nodiscard]] bool selfTest(const juce::File &payload, const juce::File &testRoot);
} // namespace forge::installer
