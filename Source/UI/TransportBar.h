#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace drawsynth
{
    class TransportBar : public juce::Component
    {
    public:
        explicit TransportBar (DrawSynthAudioProcessor& processor);

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

        void exportMidi();

        DrawSynthAudioProcessor& processorRef;

        juce::TextButton playButton { "Play" };
        juce::TextButton recordButton { "Record" };
        juce::TextButton clearButton { "Clear" };
        juce::TextButton undoButton { "Undo" };
        juce::TextButton exportMidiButton { "Export MIDI..." };
        juce::Label statusLabel;

        std::unique_ptr<ButtonAttachment> playAttachment, recordAttachment;
        std::unique_ptr<juce::FileChooser> fileChooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
    };
}
