#pragma once

#include <vector>
#include "Scale.h"

namespace drawsynth
{
    // Maps the canvas Y axis onto a quantized pitch grid built from a root,
    // a scale/mode, and an octave window. This is the ONLY place that knows
    // how "vertical position" becomes "MIDI note" - the canvas/stroke model
    // never stores a note number directly, so changing key/scale/octave here
    // retroactively re-maps every existing stroke (see StrokeModel / NoteTimelineBuilder).
    //
    // normalizedY convention: 0.0 = top of canvas = highest pitch in range,
    //                          1.0 = bottom of canvas = lowest pitch in range.
    class ScaleQuantizer
    {
    public:
        ScaleQuantizer();

        void setRoot (int pitchClass);                       // 0=C .. 11=B
        void setScale (ScaleType type);
        void setOctaveRange (int numOctaves);                 // how many octaves the grid spans (>=1)
        void setOctaveOffset (int offsetFromDefaultCentre);   // shifts the whole window up/down in octaves

        int getRoot() const noexcept { return root; }
        ScaleType getScale() const noexcept { return scale; }
        int getOctaveRange() const noexcept { return octaveRange; }
        int getOctaveOffset() const noexcept { return octaveOffset; }

        // Number of horizontal grid rows/lines the canvas should draw (>= 2).
        int getNumSteps() const;

        // stepIndexFromTop: 0 = highest note in range, getNumSteps()-1 = lowest.
        int getMidiNoteForStep (int stepIndexFromTop) const;

        // Forward mapping used when building the note timeline from a stroke.
        int quantizeNormalizedYToStep (float normalizedY) const;
        int quantizeNormalizedYToMidiNote (float normalizedY) const;

        // Inverse mapping used when recording incoming MIDI onto the canvas.
        float normalizedYForMidiNote (int midiNote) const;

        // True if candidateNote's pitch class is the current root (used by the
        // UI to draw a brighter grid line on root-note rows).
        bool isRootRow (int stepIndexFromTop) const;

    private:
        void rebuildNoteTable();

        int root = 0;
        ScaleType scale = ScaleType::Major;
        int octaveRange = 2;
        int octaveOffset = 0;

        std::vector<int> notesAscending; // low -> high MIDI note numbers in the current window
    };
}
