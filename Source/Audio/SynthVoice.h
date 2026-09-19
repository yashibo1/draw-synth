#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace drawsynth
{
    enum class OscType { Sine = 0, Saw = 1, Triangle = 2 };

    // A single monophonic oscillator + ADSR voice. Deliberately NOT a
    // juce::SynthesiserVoice: the legato behaviour this instrument needs -
    // glide to a new pitch without re-triggering the envelope, driven
    // directly by grid-quantized NoteEvents rather than MIDI messages -
    // doesn't map cleanly onto JUCE's Synthesiser voice-stealing model, so
    // it's simpler and more transparent to own this directly (see
    // SynthEngine, which is the only thing that talks to this class).
    class SynthVoice
    {
    public:
        void prepare (double sampleRate);
        void reset();

        void setOscillatorType (OscType type) noexcept { oscType = type; }
        void setAdsrParameters (const juce::ADSR::Parameters& params);

        // legato == true : glide to the new pitch, envelope keeps running (no re-attack).
        //                  Used for a note that continues an unbroken pen stroke.
        // legato == false: (re)trigger the envelope from the attack stage, pitch jumps
        //                  immediately. Used for the first note of a stroke, or after a rest.
        void startNote (int midiNote, float velocity01, bool legato);
        void stopNote();

        bool isActive() const noexcept { return adsr.isActive(); }

        // Adds (not overwrites) audio into every channel of buffer, so the
        // caller can sum multiple voices or apply a shared post-gain first.
        void renderAdding (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    private:
        float nextOscillatorSample();
        static double midiNoteToFrequency (int note) noexcept;

        double sampleRate = 44100.0;
        double phase = 0.0;

        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> smoothedFrequency;
        OscType oscType = OscType::Saw;
        float currentVelocity = 0.8f;

        juce::ADSR adsr;
        juce::ADSR::Parameters adsrParams;
    };
}
