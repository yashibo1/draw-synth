#include "PluginEditor.h"

namespace drawsynth
{
    DrawSynthAudioProcessorEditor::DrawSynthAudioProcessorEditor (DrawSynthAudioProcessor& p)
        : AudioProcessorEditor (&p), processorRef (p),
          topControlBar (p), canvas (p), transportBar (p)
    {
        addAndMakeVisible (topControlBar);
        addAndMakeVisible (canvas);
        addAndMakeVisible (transportBar);

        setResizable (true, true);
        setResizeLimits (760, 480, 1800, 1200);
        setSize (980, 640);
    }

    DrawSynthAudioProcessorEditor::~DrawSynthAudioProcessorEditor() = default;

    void DrawSynthAudioProcessorEditor::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff1a1c22));
    }

    void DrawSynthAudioProcessorEditor::resized()
    {
        auto area = getLocalBounds();
        topControlBar.setBounds (area.removeFromTop (84));
        transportBar.setBounds (area.removeFromBottom (56));
        canvas.setBounds (area.reduced (8));
    }
}
