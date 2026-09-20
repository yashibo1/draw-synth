#pragma once

#include "SynthVoice.h"
#include "../Canvas/NoteTimeline.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <map>

namespace drawsynth
{
    // Consumes a NoteTimeline and, for one processBlock's worth of samples,
    // both renders internal-synth audio (sample-accurate, splitting at every
    // note-on/note-off boundary) AND emits the equivalent MIDI note-on/off
    // messages into an outgoing MidiBuffer for live MIDI-out passthrough.
    // Audio and MIDI-out are always in lockstep because both come from the
    // same single walk over the same timeline.
    //
    // Each NoteEvent belongs to a "lane" (one per drawn stroke - see
    // NoteTimeline.h). Every lane is inherently monophonic along its own
    // path (that's what makes legato/glide well-defined), but several lanes
    // can sound at once - that's a chord. A small fixed pool of voices makes
    // this possible: each currently-sounding lane holds one voice, legato
    // continues on that same voice, and a lane that ends frees its voice for
    // reuse (or, if every voice is busy, the least-recently-triggered voice
    // is stolen).
    class SynthEngine
    {
    public:
        static constexpr int kMaxVoices = 16;

        void prepare (double sampleRate);
        void reset();

        void setOscillatorType (OscType type);
        void setAdsrParameters (const juce::ADSR::Parameters& params);

        // phaseBeatsAtBlockStart must already be wrapped into [0, timeline.loopLengthBeats).
        // audioOut's existing content is added to, not overwritten - the caller clears first.
        void renderBlock (const NoteTimeline& timeline,
                           double phaseBeatsAtBlockStart,
                           double beatsPerSample,
                           juce::AudioBuffer<float>& audioOut,
                           juce::MidiBuffer& midiOut);

        // Forces every sounding voice off immediately and emits matching
        // MIDI note-offs. Called when playback stops or the loop wraps.
        void allNotesOff (juce::MidiBuffer& midiOut, int atSample = 0);

        // Renders whatever release tails are still active, without scheduling
        // anything new. Used while transport is stopped so a note doesn't cut
        // off with a click.
        void renderTailOnly (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    private:
        struct Action
        {
            double beat;
            bool isStart;
            int midiNote;
            int colorIndex;
            int laneId;
            float velocity01;
            bool legato;
        };

        struct VoiceSlot
        {
            SynthVoice voice;
            int assignedLaneId = -1;   // -1 = free
            int currentMidiNote = -1;
            int currentColorIndex = 0;
            uint64_t triggerOrder = 0; // for least-recently-triggered voice stealing
        };

        int findOrAllocateVoiceForLane (int laneId, bool preferExistingLaneVoice);
        void processSegment (const NoteTimeline& timeline,
                              double segStartBeat, double segEndBeat,
                              int destOffset, int segLengthSamples,
                              double beatsPerSample,
                              juce::AudioBuffer<float>& audioOut,
                              juce::MidiBuffer& midiOut);

        std::array<VoiceSlot, kMaxVoices> voiceSlots;
        std::map<int, int> laneToVoiceSlot; // laneId -> index into voiceSlots
        uint64_t triggerCounter = 0;
    };
}
