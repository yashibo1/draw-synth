#pragma once

#include <vector>

namespace drawsynth
{
    // A single quantized note, ready to play. This is the interchange format
    // that decouples "how the canvas was drawn" from "what plays it":
    //   - SynthEngine consumes a NoteTimeline directly for internal audio + live MIDI-out.
    //   - MidiExporter consumes the same NoteTimeline to write a standalone .mid file.
    //   - A future offline WAV render would just call SynthEngine::renderBlock in a loop
    //     against this same timeline - no new data path required.
    struct NoteEvent
    {
        double startBeat = 0.0;
        double endBeat = 0.0;      // exclusive
        int midiNote = 60;
        int colorIndex = 0;        // reserved for multi-voice colors (deferred)
        float velocity01 = 0.8f;   // 0..1

        // True if this event continues an unbroken pen stroke from the
        // immediately preceding event (same stroke, no time gap, different
        // pitch). The synth engine reads this to glide instead of
        // re-triggering the envelope; MIDI output reads it to overlap the
        // new note-on slightly before the old note-off, the standard
        // convention mono-legato synths use to recognise a glide.
        bool legatoFromPrevious = false;
    };

    struct NoteTimeline
    {
        double loopLengthBeats = 16.0;
        std::vector<NoteEvent> events; // sorted ascending by startBeat, non-overlapping per color
    };
}
