#pragma once

#include "SynthVoice.h"
#include "../Canvas/NoteTimeline.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace drawsynth
{
    // Consumes a NoteTimeline and, for one processBlock's worth of samples,
    // both renders internal-synth audio (sample-accurate, splitting at every
    // note-on/note-off boundary) AND emits the equivalent MIDI note-on/off
    // messages into an outgoing MidiBuffer for live MIDI-out passthrough.
    // Audio and MIDI-out are always in lockstep because both come from the
    // same single walk over the same timeline.
    class SynthEngine
    {
    public:
        void prepare (double sampleRate);
        void reset();

        SynthVoice& getVoice() noexcept { return voice; }

        // phaseBeatsAtBlockStart must already be wrapped into [0, timeline.loopLengthBeats).
        // audioOut's existing content is added to, not overwritten - the caller clears first.
        void renderBlock (const NoteTimeline& timeline,
                           double phaseBeatsAtBlockStart,
                           double beatsPerSample,
                           juce::AudioBuffer<float>& audioOut,
                           juce::MidiBuffer& midiOut);

        // Forces the voice off immediately and emits a matching MIDI note-off
        // if a note was left sounding. Called when playback stops.
        void allNotesOff (juce::MidiBuffer& midiOut, int atSample = 0);

    private:
        struct Action
        {
            double beat;
            bool isStart;
            int midiNote;
            int colorIndex;
            float velocity01;
            bool legato;
        };

        void processSegment (const NoteTimeline& timeline,
                              double segStartBeat, double segEndBeat,
                              int destOffset, int segLengthSamples,
                              double beatsPerSample,
                              juce::AudioBuffer<float>& audioOut,
                              juce::MidiBuffer& midiOut);

        SynthVoice voice;
        int lastSoundingNote = -1;
        int lastSoundingColor = 0;
    };
}
