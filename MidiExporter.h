#pragma once

#include "../Canvas/NoteTimeline.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace drawsynth
{
    namespace MidiExporter
    {
        // Writes one loop cycle of the given timeline to a Standard MIDI
        // File, including a tempo meta-event so the file plays back at the
        // right speed wherever it's imported. This reuses the exact same
        // NoteTimeline the internal synth and live MIDI-out play from - the
        // "architecture note" that export is a trivial reuse of the same
        // timeline is literally true here, not just in spirit.
        bool exportToFile (const NoteTimeline& timeline, double bpm, const juce::File& destFile);
    }
}
