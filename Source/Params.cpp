#include "Params.h"
#include "Music/Scale.h"

namespace drawsynth
{
    namespace
    {
        juce::StringArray toStringArray (const std::vector<std::string>& in)
        {
            juce::StringArray out;
            for (const auto& s : in)
                out.add (juce::String (s));
            return out;
        }

        constexpr int kLoopBarChoices[] { 1, 2, 4, 8 };
    }

    int notesPerBeatChoiceToValue (int choiceIndex)
    {
        return juce::jlimit (1, 4, choiceIndex + 1);
    }

    int loopBarsChoiceToValue (int choiceIndex)
    {
        const int clamped = juce::jlimit (0, 3, choiceIndex);
        return kLoopBarChoices[clamped];
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> params;

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamIDs::root, 1 }, "Key", toStringArray (getAllRootNames()), 0));

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamIDs::scale, 1 }, "Scale", toStringArray (getAllScaleNames()), 0));

        params.push_back (std::make_unique<AudioParameterInt> (
            ParameterID { ParamIDs::octaveRange, 1 }, "Octave Range", 1, 4, 2));

        params.push_back (std::make_unique<AudioParameterInt> (
            ParameterID { ParamIDs::octaveOffset, 1 }, "Octave Offset", -2, 2, 0));

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamIDs::notesPerBeat, 1 }, "Quantize",
            StringArray { "1 / beat", "2 / beat", "3 / beat (triplet)", "4 / beat" }, 1));

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamIDs::loopBars, 1 }, "Loop Length",
            StringArray { "1 bar", "2 bars", "4 bars", "8 bars" }, 2));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { ParamIDs::tempoSync, 1 }, "Sync To Host", true));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::manualBpm, 1 }, "Manual BPM",
            NormalisableRange<float> (20.0f, 300.0f, 0.1f), 120.0f));

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamIDs::oscType, 1 }, "Oscillator",
            StringArray { "Sine", "Saw", "Triangle" }, 1));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::attack, 1 }, "Attack",
            NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.02f));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::decay, 1 }, "Decay",
            NormalisableRange<float> (0.001f, 2.0f, 0.001f, 0.4f), 0.1f));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::sustain, 1 }, "Sustain",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::release, 1 }, "Release",
            NormalisableRange<float> (0.001f, 3.0f, 0.001f, 0.4f), 0.2f));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamIDs::masterGain, 1 }, "Level",
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { ParamIDs::playing, 1 }, "Play", false));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { ParamIDs::recording, 1 }, "Record", false));

        return { params.begin(), params.end() };
    }
}
