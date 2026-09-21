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

        // Which stroke this event came from. Each stroke is its own
        // independent monophonic "lane" - legato/glide only ever links
        // events within the same lane, and the synth engine gives each
        // concurrently-sounding lane its own voice, so two overlapping
        // strokes sound as a chord instead of one silently replacing the
        // other.
        int laneId = 0;

        // True if this event continues an unbroken pen stroke from the
        // immediately preceding event *in the same lane* (no time gap,
        // different pitch). The synth engine reads this to glide instead of
        // re-triggering the envelope; MIDI output reads it to overlap the
        // new note-on slightly before the old note-off, the standard
        // convention mono-legato synths use to recognise a glide.
        bool legatoFromPrevious = false;

        // True if the *next* event in this same lane starts exactly where
        // this one ends with legatoFromPrevious set - i.e. this event's own
        // end is superseded by that next event's start, so nothing should
        // schedule an explicit note-off for it. Computed once at build time
        // (see NoteTimelineBuilder) since, once lanes are merged into one
        // globally time-sorted list, a lane's own next event is no longer
        // necessarily the next array element.
        bool hasSeamlessSuccessor = false;
    };

    struct NoteTimeline
    {
        double loopLengthBeats = 16.0;
        std::vector<NoteEvent> events; // sorted ascending by startBeat; events in different lanes may overlap (chords)
    };
}
