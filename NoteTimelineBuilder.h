#pragma once

#include "StrokeModel.h"
#include "NoteTimeline.h"
#include "../Music/ScaleQuantizer.h"

namespace drawsynth
{
    // Converts the raw, unquantized StrokeModel into a quantized NoteTimeline,
    // using the *current* ScaleQuantizer/grid settings. Called again from
    // scratch whenever strokes or quantization settings change - this is what
    // makes "quantize on the fly" true: nothing about the quantization is
    // ever baked into the stored strokes.
    class NoteTimelineBuilder
    {
    public:
        // notesPerBeat: horizontal grid resolution (grid cell = 1/notesPerBeat of a beat).
        // loopLengthBeats: length of one loop cycle, in beats (bars * beatsPerBar).
        static NoteTimeline build (const std::vector<Stroke>& strokes,
                                    const ScaleQuantizer& quantizer,
                                    double loopLengthBeats,
                                    int notesPerBeat);
    };
}
