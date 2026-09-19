#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/CanvasComponent.h"
#include "UI/TopControlBar.h"
#include "UI/TransportBar.h"

namespace drawsynth
{
    class DrawSynthAudioProcessorEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit DrawSynthAudioProcessorEditor (DrawSynthAudioProcessor&);
        ~DrawSynthAudioProcessorEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        DrawSynthAudioProcessor& processorRef;

        TopControlBar topControlBar;
        CanvasComponent canvas;
        TransportBar transportBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawSynthAudioProcessorEditor)
    };
}
