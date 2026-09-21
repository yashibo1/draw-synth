#include "SynthVoice.h"

namespace drawsynth
{
    namespace
    {
        constexpr double kGlideTimeSeconds = 0.05;
    }

    void SynthVoice::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

        adsr.setSampleRate (sampleRate);
        adsr.setParameters (adsrParams);

        smoothedFrequency.reset (sampleRate, kGlideTimeSeconds);
        smoothedFrequency.setCurrentAndTargetValue (midiNoteToFrequency (60));

        phase = 0.0;
    }

    void SynthVoice::reset()
    {
        adsr.reset();
        phase = 0.0;
    }

    void SynthVoice::setAdsrParameters (const juce::ADSR::Parameters& params)
    {
        adsrParams = params;
        adsr.setParameters (adsrParams);
    }

    void SynthVoice::startNote (int midiNote, float velocity01, bool legato)
    {
        const double targetFreq = midiNoteToFrequency (midiNote);
        currentVelocity = juce::jlimit (0.0f, 1.0f, velocity01);

        if (legato && adsr.isActive())
        {
            // Continue the current envelope stage, just glide the pitch.
            smoothedFrequency.setTargetValue (targetFreq);
        }
        else
        {
            // Fresh trigger: pitch jumps immediately, envelope restarts from
            // the attack stage. This also correctly handles the case of two
            // back-to-back non-legato notes with no gap between them.
            smoothedFrequency.setCurrentAndTargetValue (targetFreq);
            adsr.noteOn();
        }
    }

    void SynthVoice::stopNote()
    {
        adsr.noteOff();
    }

    float SynthVoice::nextOscillatorSample()
    {
        const double freq = smoothedFrequency.getNextValue();
        const double phaseIncrement = freq / sampleRate;

        float sample = 0.0f;
        switch (oscType)
        {
            case OscType::Sine:
                sample = static_cast<float> (std::sin (phase * juce::MathConstants<double>::twoPi));
                break;

            case OscType::Saw:
                // Naive (non-bandlimited) rising sawtooth: simple and
                // adequate for a v1 built-in synth, at the cost of some
                // aliasing on high notes - a good spot for a future
                // polyBLEP upgrade without touching anything else.
                sample = static_cast<float> (2.0 * phase - 1.0);
                break;

            case OscType::Triangle:
                sample = static_cast<float> (4.0 * std::abs (phase - 0.5) - 1.0);
                break;
        }

        phase += phaseIncrement;
        if (phase >= 1.0)
            phase -= 1.0;

        return sample;
    }

    void SynthVoice::renderAdding (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const int numChannels = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const float osc = nextOscillatorSample();
            const float env = adsr.getNextSample();
            const float sample = osc * env * currentVelocity;

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.addSample (ch, startSample + i, sample);
        }
    }

    double SynthVoice::midiNoteToFrequency (int note) noexcept
    {
        return 440.0 * std::pow (2.0, (static_cast<double> (note) - 69.0) / 12.0);
    }
}
