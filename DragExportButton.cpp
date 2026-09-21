#include "DragExportButton.h"

namespace drawsynth
{
    DragExportButton::DragExportButton (DrawSynthAudioProcessor& processor) : processorRef (processor)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Press and drag this into your DAW's timeline or piano roll");
    }

    void DragExportButton::mouseDown (const juce::MouseEvent&)
    {
        dragHasStarted = false;
    }

    void DragExportButton::mouseEnter (const juce::MouseEvent&)
    {
        hovering = true;
        repaint();
    }

    void DragExportButton::mouseExit (const juce::MouseEvent&)
    {
        hovering = false;
        repaint();
    }

    void DragExportButton::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragHasStarted || e.getDistanceFromDragStart() < 6)
            return;

        auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (dragContainer == nullptr)
            return;

        // A fixed name (rather than a fresh one per drag) is fine: each drag
        // is a discrete user action, and by the time a second one happens
        // the DAW has already finished reading the first drop.
        auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("DrawSynth_Pattern.mid");

        if (! processorRef.exportPatternAsMidiFile (tempFile))
            return;

        dragHasStarted = true;

        juce::StringArray files;
        files.add (tempFile.getFullPathName());
        dragContainer->performExternalDragDropOfFiles (files, false);
    }

    void DragExportButton::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (juce::Colour (0xff3a3f4a).withAlpha (hovering ? 1.0f : 0.85f));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.drawText ("Drag into DAW", bounds, juce::Justification::centred);
    }
}
