#include "PluginEditor.h"

namespace drawsynth
{
    DrawSynthAudioProcessorEditor::DrawSynthAudioProcessorEditor (DrawSynthAudioProcessor& p)
        : AudioProcessorEditor (&p), processorRef (p),
          presetBar (p), topControlBar (p), canvas (p), transportBar (p, canvas)
    {
        addAndMakeVisible (presetBar);
        addAndMakeVisible (topControlBar);
        addAndMakeVisible (canvas);
        addAndMakeVisible (transportBar);

        setResizable (true, true);
        setResizeLimits (760, 520, 1800, 1200);
        setSize (980, 680);
    }

    DrawSynthAudioProcessorEditor::~DrawSynthAudioProcessorEditor() = default;

    void DrawSynthAudioProcessorEditor::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff1a1c22));
    }

    void DrawSynthAudioProcessorEditor::resized()
    {
        auto area = getLocalBounds();
        presetBar.setBounds (area.removeFromTop (36));
        topControlBar.setBounds (area.removeFromTop (84));
        transportBar.setBounds (area.removeFromBottom (56));
        canvas.setBounds (area.reduced (8));
    }
}
