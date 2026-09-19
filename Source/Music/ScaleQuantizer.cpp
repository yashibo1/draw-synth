#include "ScaleQuantizer.h"
#include <algorithm>
#include <cmath>

namespace drawsynth
{
    namespace
    {
        // MIDI note for the root pitch class in the lowest octave of the
        // default (offset = 0) window. Chosen so the default 2-octave range
        // sits comfortably around middle C. The exact octave-naming
        // convention doesn't matter anywhere else in the codebase - only
        // relative movement and the root pitch class are ever surfaced to
        // the user.
        constexpr int kDefaultWindowBottom = 48;

        int wrapMod (int value, int modulus)
        {
            const int m = value % modulus;
            return m < 0 ? m + modulus : m;
        }
    }

    ScaleQuantizer::ScaleQuantizer()
    {
        rebuildNoteTable();
    }

    void ScaleQuantizer::setRoot (int pitchClass)
    {
        root = wrapMod (pitchClass, 12);
        rebuildNoteTable();
    }

    void ScaleQuantizer::setScale (ScaleType type)
    {
        scale = type;
        rebuildNoteTable();
    }

    void ScaleQuantizer::setOctaveRange (int numOctaves)
    {
        octaveRange = std::max (1, numOctaves);
        rebuildNoteTable();
    }

    void ScaleQuantizer::setOctaveOffset (int offsetFromDefaultCentre)
    {
        octaveOffset = offsetFromDefaultCentre;
        rebuildNoteTable();
    }

    void ScaleQuantizer::rebuildNoteTable()
    {
        notesAscending.clear();

        const auto& intervals = getScaleIntervals (scale);
        const int stepsPerOctave = static_cast<int> (intervals.size());
        const int lowestRootNote = kDefaultWindowBottom + octaveOffset * 12 + root;

        for (int oct = 0; oct < octaveRange; ++oct)
            for (int deg = 0; deg < stepsPerOctave; ++deg)
                notesAscending.push_back (lowestRootNote + oct * 12 + intervals[static_cast<size_t> (deg)]);

        // Close the window with the top root so a 2-octave range visually
        // and musically spans exactly two octaves, root to root.
        notesAscending.push_back (lowestRootNote + octaveRange * 12);
    }

    int ScaleQuantizer::getNumSteps() const
    {
        return static_cast<int> (notesAscending.size());
    }

    int ScaleQuantizer::getMidiNoteForStep (int stepIndexFromTop) const
    {
        if (notesAscending.empty())
            return 60;

        const int n = getNumSteps();
        const int clamped = std::clamp (stepIndexFromTop, 0, n - 1);
        return notesAscending[static_cast<size_t> (n - 1 - clamped)];
    }

    int ScaleQuantizer::quantizeNormalizedYToStep (float normalizedY) const
    {
        const int n = getNumSteps();
        if (n <= 1)
            return 0;

        const float clampedY = std::clamp (normalizedY, 0.0f, 1.0f);
        const int step = static_cast<int> (std::lround (clampedY * static_cast<float> (n - 1)));
        return std::clamp (step, 0, n - 1);
    }

    int ScaleQuantizer::quantizeNormalizedYToMidiNote (float normalizedY) const
    {
        return getMidiNoteForStep (quantizeNormalizedYToStep (normalizedY));
    }

    float ScaleQuantizer::normalizedYForMidiNote (int midiNote) const
    {
        if (notesAscending.empty())
            return 0.5f;

        // Find the closest note in the current window (recorded MIDI input
        // may fall outside the visible range if the user changed the octave
        // window after recording - clamp gracefully rather than throw it away).
        size_t bestIndex = 0;
        int bestDistance = std::abs (notesAscending[0] - midiNote);
        for (size_t i = 1; i < notesAscending.size(); ++i)
        {
            const int distance = std::abs (notesAscending[i] - midiNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        const int n = getNumSteps();
        const int stepIndexFromTop = n - 1 - static_cast<int> (bestIndex);
        return n > 1 ? static_cast<float> (stepIndexFromTop) / static_cast<float> (n - 1) : 0.5f;
    }

    bool ScaleQuantizer::isRootRow (int stepIndexFromTop) const
    {
        return wrapMod (getMidiNoteForStep (stepIndexFromTop) - root, 12) == 0;
    }
}
