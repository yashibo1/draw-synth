#pragma once

#include <vector>
#include <cstddef>

namespace drawsynth
{
    // A single recorded point of a pen stroke. Deliberately stores RAW,
    // unquantized data - no MIDI note number, no grid-cell index - so that
    // changing key/scale/octave range later re-maps existing drawings
    // instead of needing the strokes to be redrawn. Quantization happens on
    // the fly in NoteTimelineBuilder using the current ScaleQuantizer.
    struct StrokePoint
    {
        double timeBeats = 0.0;     // position along the loop, in beats, unquantized
        float normalizedY = 0.5f;   // 0 = top (highest pitch) .. 1 = bottom (lowest pitch), unquantized
        float pressure = 0.8f;      // 0..1, drives note velocity; defaults to a sensible constant for a plain mouse
    };

    struct Stroke
    {
        std::vector<StrokePoint> points;
        int colorIndex = 0; // voice/instrument layer. v1 always uses 0; the field exists so
                             // multi-voice colors (deferred) is a data-model no-op to add later.
    };

    // Owns the set of strokes that make up the drawn pattern. This class is
    // message-thread-only: all mutation happens from mouse events (or the
    // MIDI-record path draining on a timer), never from the audio thread.
    // It has no notion of "what plays" - that's NoteTimelineBuilder's job.
    class StrokeModel
    {
    public:
        void beginStroke (double timeBeats, float normalizedY, float pressure, int colorIndex);
        void continueStroke (double timeBeats, float normalizedY, float pressure);
        void endStroke();

        void clear();
        void undoLastStroke();

        const std::vector<Stroke>& getStrokes() const noexcept { return strokes; }
        bool isEmpty() const noexcept { return strokes.empty(); }

    private:
        std::vector<Stroke> strokes;
        int activeStrokeIndex = -1;
    };
}
