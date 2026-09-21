#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/CanvasComponent.h"
#include "UI/TopControlBar.h"
#include "UI/PresetBar.h"
#include "UI/TransportBar.h"

namespace drawsynth
{
    // Also a DragAndDropContainer: this is what lets DragExportButton (in the
    // transport bar) find an ancestor via findParentDragContainerFor() and
    // hand a file to the OS's drag-and-drop system, dropping the pattern
    // straight into the host's timeline or piano roll.
    class DrawSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::DragAndDropContainer
    {
    public:
        explicit DrawSynthAudioProcessorEditor (DrawSynthAudioProcessor&);
        ~DrawSynthAudioProcessorEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        DrawSynthAudioProcessor& processorRef;

        juce::TooltipWindow tooltipWindow { this };

        PresetBar presetBar;
        TopControlBar topControlBar;
        CanvasComponent canvas;
        TransportBar transportBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawSynthAudioProcessorEditor)
    };
}
