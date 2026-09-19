#include "Scale.h"

namespace drawsynth
{
    const std::vector<int>& getScaleIntervals (ScaleType type)
    {
        static const std::vector<int> major          { 0, 2, 4, 5, 7, 9, 11 };
        static const std::vector<int> naturalMinor    { 0, 2, 3, 5, 7, 8, 10 };
        static const std::vector<int> dorian          { 0, 2, 3, 5, 7, 9, 10 };
        static const std::vector<int> phrygian        { 0, 1, 3, 5, 7, 8, 10 };
        static const std::vector<int> lydian          { 0, 2, 4, 6, 7, 9, 11 };
        static const std::vector<int> mixolydian      { 0, 2, 4, 5, 7, 9, 10 };
        static const std::vector<int> locrian         { 0, 1, 3, 5, 6, 8, 10 };
        static const std::vector<int> harmonicMinor   { 0, 2, 3, 5, 7, 8, 11 };
        static const std::vector<int> melodicMinor    { 0, 2, 3, 5, 7, 9, 11 };

        switch (type)
        {
            case ScaleType::Major:         return major;
            case ScaleType::NaturalMinor:  return naturalMinor;
            case ScaleType::Dorian:        return dorian;
            case ScaleType::Phrygian:      return phrygian;
            case ScaleType::Lydian:        return lydian;
            case ScaleType::Mixolydian:    return mixolydian;
            case ScaleType::Locrian:       return locrian;
            case ScaleType::HarmonicMinor: return harmonicMinor;
            case ScaleType::MelodicMinor:  return melodicMinor;
            case ScaleType::NumScaleTypes: break; // sentinel, not a real scale
        }
        return major;
    }

    std::string getScaleName (ScaleType type)
    {
        switch (type)
        {
            case ScaleType::Major:         return "Major";
            case ScaleType::NaturalMinor:  return "Natural Minor";
            case ScaleType::Dorian:        return "Dorian";
            case ScaleType::Phrygian:      return "Phrygian";
            case ScaleType::Lydian:        return "Lydian";
            case ScaleType::Mixolydian:    return "Mixolydian";
            case ScaleType::Locrian:       return "Locrian";
            case ScaleType::HarmonicMinor: return "Harmonic Minor";
            case ScaleType::MelodicMinor:  return "Melodic Minor";
            case ScaleType::NumScaleTypes: break; // sentinel, not a real scale
        }
        return "Major";
    }

    std::string getRootName (int pitchClass)
    {
        static const std::array<const char*, 12> names {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        return names[static_cast<size_t> (((pitchClass % 12) + 12) % 12)];
    }

    std::vector<std::string> getAllScaleNames()
    {
        std::vector<std::string> names;
        for (int i = 0; i < static_cast<int> (ScaleType::NumScaleTypes); ++i)
            names.push_back (getScaleName (static_cast<ScaleType> (i)));
        return names;
    }

    std::vector<std::string> getAllRootNames()
    {
        std::vector<std::string> names;
        for (int i = 0; i < 12; ++i)
            names.push_back (getRootName (i));
        return names;
    }
}
