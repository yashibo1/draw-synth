#pragma once

#include <array>
#include <string>
#include <vector>

// Pure, framework-independent music theory data. Deliberately has zero JUCE
// dependency so it can be unit-tested with a plain compiler and reused
// anywhere (e.g. a future mobile companion app) without dragging in JUCE.
namespace drawsynth
{
    enum class ScaleType
    {
        Major = 0,
        NaturalMinor,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Locrian,
        HarmonicMinor,
        MelodicMinor,
        NumScaleTypes
    };

    // Semitone offsets from the root, ascending, within one octave.
    const std::vector<int>& getScaleIntervals (ScaleType type);

    std::string getScaleName (ScaleType type);

    // 0 = C, 1 = C#, ... 11 = B
    std::string getRootName (int pitchClass);

    // Convenience for UI population.
    std::vector<std::string> getAllScaleNames();
    std::vector<std::string> getAllRootNames();
}
