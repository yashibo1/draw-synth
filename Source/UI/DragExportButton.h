#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace drawsynth
{
    // A drag handle for the current pattern: press and drag it out of the
    // plugin window and drop it directly onto a DAW's timeline or piano roll
    // (FL Studio and most other hosts accept an OS-level file drag of a
    // .mid file) - no file-save dialog, no download step. Uses the same
    // MidiExporter::exportToFile the "Export MIDI..." button uses, just
    // writing to a temp file instead of a user-chosen one.
    //
    // Requires a juce::DragAndDropContainer somewhere up the component tree
    // (the plugin editor implements this - see PluginEditor.h) since that's
    // what actually knows how to hand a file to the OS's drag-and-drop system.
    class DragExportButton : public juce::Component,
                              public juce::SettableTooltipClient
    {
    public:
        explicit DragExportButton (DrawSynthAudioProcessor& processor);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        DrawSynthAudioProcessor& processorRef;
        bool dragHasStarted = false;
        bool hovering = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragExportButton)
    };
}
