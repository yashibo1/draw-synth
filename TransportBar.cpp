#include "TransportBar.h"
#include "../Params.h"
#include "../BuildInfo.h"

namespace drawsynth
{
    TransportBar::TransportBar (DrawSynthAudioProcessor& processor, CanvasComponent& canvasToControl)
        : processorRef (processor), canvasRef (canvasToControl), dragExportButton (processor)
    {
        auto& apvts = processor.parameters;

        playButton.setClickingTogglesState (true);
        playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4caf7d));
        addAndMakeVisible (playButton);
        playAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::playing, playButton);

        recordButton.setClickingTogglesState (true);
        recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd9534f));
        addAndMakeVisible (recordButton);
        recordAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::recording, recordButton);

        eraserButton.setClickingTogglesState (true);
        eraserButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0a63a));
        eraserButton.onClick = [this] { canvasRef.setEraseMode (eraserButton.getToggleState()); };
        addAndMakeVisible (eraserButton);

        clearButton.onClick = [this] { processorRef.clearCanvas(); };
        addAndMakeVisible (clearButton);

        undoButton.onClick = [this] { processorRef.undoLastStroke(); };
        addAndMakeVisible (undoButton);

        exportMidiButton.onClick = [this] { exportMidi(); };
        addAndMakeVisible (exportMidiButton);

        addAndMakeVisible (dragExportButton);

        statusLabel.setJustificationType (juce::Justification::centredRight);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        statusLabel.setText ("Draw on the canvas, then hit Play", juce::dontSendNotification);
        addAndMakeVisible (statusLabel);

        buildLabel.setJustificationType (juce::Justification::centredRight);
        buildLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffcc66));
        buildLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        buildLabel.setText (kBuildTag, juce::dontSendNotification);
        buildLabel.setTooltip ("Confirms which build is actually running - check this matches what you expect if a fix doesn't seem to be taking effect.");
        addAndMakeVisible (buildLabel);
    }

    void TransportBar::exportMidi()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Export pattern as MIDI file",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DrawSynthPattern.mid"),
            "*.mid");

        constexpr auto chooserFlags = juce::FileBrowserComponent::saveMode
                                       | juce::FileBrowserComponent::canSelectFiles
                                       | juce::FileBrowserComponent::warnAboutOverwriting;

        fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File {})
                return;

            const bool ok = processorRef.exportPatternAsMidiFile (file);
            statusLabel.setText (ok ? ("Exported " + file.getFileName()) : juce::String ("Export failed"),
                                 juce::dontSendNotification);
        });
    }

    void TransportBar::paint (juce::Graphics& g)
    {
        g.setColour (juce::Colour (0xff23262e));
        g.fillRect (getLocalBounds());
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);
    }

    void TransportBar::resized()
    {
        auto area = getLocalBounds().reduced (8, 6);
        playButton.setBounds (area.removeFromLeft (70));
        area.removeFromLeft (8);
        recordButton.setBounds (area.removeFromLeft (80));
        area.removeFromLeft (16);
        eraserButton.setBounds (area.removeFromLeft (70));
        area.removeFromLeft (8);
        clearButton.setBounds (area.removeFromLeft (60));
        area.removeFromLeft (8);
        undoButton.setBounds (area.removeFromLeft (60));
        area.removeFromLeft (16);
        exportMidiButton.setBounds (area.removeFromLeft (130));
        area.removeFromLeft (8);
        dragExportButton.setBounds (area.removeFromLeft (110));
        area.removeFromLeft (12);
        buildLabel.setBounds (area.removeFromRight (130));
        statusLabel.setBounds (area);
    }
}
