#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "CanvasComponent.h"
#include "DragExportButton.h"

namespace drawsynth
{
    class TransportBar : public juce::Component
    {
    public:
        TransportBar (DrawSynthAudioProcessor& processor, CanvasComponent& canvasToControl);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

        void exportMidi();

        DrawSynthAudioProcessor& processorRef;
        CanvasComponent& canvasRef;

        juce::TextButton playButton { "Play" };
        juce::TextButton recordButton { "Record" };
        juce::TextButton eraserButton { "Eraser" };
        juce::TextButton clearButton { "Clear" };
        juce::TextButton undoButton { "Undo" };
        juce::TextButton exportMidiButton { "Export MIDI..." };
        DragExportButton dragExportButton;
        juce::Label statusLabel;

        std::unique_ptr<ButtonAttachment> playAttachment, recordAttachment;
        std::unique_ptr<juce::FileChooser> fileChooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
    };
}
