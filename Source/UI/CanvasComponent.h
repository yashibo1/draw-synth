#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace drawsynth
{
    // The centrepiece of the plugin. X = time (one loop cycle), Y = pitch
    // (quantized to the current key/scale/octave window). Mouse gestures
    // become raw, unquantized StrokePoints via the processor's
    // beginStroke/continueStroke/endStroke API; the bright overlay blocks
    // show what will actually play, reading the very same NoteTimeline the
    // audio engine consumes - so what you see here is exactly what you hear.
    class CanvasComponent : public juce::Component, private juce::Timer
    {
    public:
        explicit CanvasComponent (DrawSynthAudioProcessor& processor);
        ~CanvasComponent() override;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;

        double xToBeats (int x) const;
        float yToNormalized (int y) const;
        float beatsToX (double beats, double loopLength, float width) const;
        static float pressureFromEvent (const juce::MouseEvent& e);

        DrawSynthAudioProcessor& processorRef;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanvasComponent)
    };
}
