#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace drawsynth
{
    // Central list of every automatable parameter ID. Kept as one file so
    // there's a single place to check when wiring up UI controls or reading
    // values in the audio thread.
    namespace ParamIDs
    {
        constexpr const char* root          = "root";
        constexpr const char* scale         = "scale";
        constexpr const char* octaveRange   = "octaveRange";
        constexpr const char* octaveOffset  = "octaveOffset";
        constexpr const char* notesPerBeat  = "notesPerBeat";
        constexpr const char* loopBars      = "loopBars";
        constexpr const char* tempoSync     = "tempoSync";
        constexpr const char* manualBpm     = "manualBpm";
        constexpr const char* oscType       = "oscType";
        constexpr const char* attack        = "attack";
        constexpr const char* decay         = "decay";
        constexpr const char* sustain       = "sustain";
        constexpr const char* release       = "release";
        constexpr const char* masterGain    = "masterGain";
        constexpr const char* playing       = "playing";
        constexpr const char* recording     = "recording";

        // Parameters that change the *shape* of the note-event grid and
        // therefore require the NoteTimeline to be rebuilt when they change.
        constexpr const char* timelineAffectingIDs[] {
            root, scale, octaveRange, octaveOffset, notesPerBeat, loopBars
        };
    }

    // notesPerBeat / loopBars are exposed as choices of human-readable
    // labels; these helpers turn a chosen index back into the integer value
    // used by the music engine.
    int notesPerBeatChoiceToValue (int choiceIndex);
    int loopBarsChoiceToValue (int choiceIndex);

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
