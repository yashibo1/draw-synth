#include "CanvasComponent.h"
#include <cmath>

namespace drawsynth
{
    CanvasComponent::CanvasComponent (DrawSynthAudioProcessor& processor) : processorRef (processor)
    {
        setWantsKeyboardFocus (false);
        startTimerHz (30); // animates the playhead; strokes/grid repaint immediately on edit instead
    }

    CanvasComponent::~CanvasComponent()
    {
        stopTimer();
    }

    void CanvasComponent::timerCallback()
    {
        if (processorRef.isTransportPlaying())
            repaint();
    }

    double CanvasComponent::xToBeats (int x) const
    {
        const int w = juce::jmax (1, getWidth());
        const double fraction = juce::jlimit (0.0, 1.0, static_cast<double> (x) / static_cast<double> (w));
        return fraction * processorRef.getLoopLengthBeats();
    }

    float CanvasComponent::yToNormalized (int y) const
    {
        const int h = juce::jmax (1, getHeight());
        return juce::jlimit (0.0f, 1.0f, static_cast<float> (y) / static_cast<float> (h));
    }

    float CanvasComponent::beatsToX (double beats, double loopLength, float width) const
    {
        if (loopLength <= 0.0)
            return 0.0f;

        const double fraction = juce::jlimit (0.0, 1.0, beats / loopLength);
        return static_cast<float> (fraction * width);
    }

    float CanvasComponent::pressureFromEvent (const juce::MouseEvent& e)
    {
        const float p = e.pressure;
        return (p > 0.0f && p < 1.0f) ? p : 0.8f;
    }

    void CanvasComponent::mouseDown (const juce::MouseEvent& e)
    {
        processorRef.beginStroke (xToBeats (e.x), yToNormalized (e.y), pressureFromEvent (e));
        repaint();
    }

    void CanvasComponent::mouseDrag (const juce::MouseEvent& e)
    {
        processorRef.continueStroke (xToBeats (e.x), yToNormalized (e.y), pressureFromEvent (e));
        repaint();
    }

    void CanvasComponent::mouseUp (const juce::MouseEvent&)
    {
        processorRef.endStroke();
        repaint();
    }

    void CanvasComponent::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.fillAll (juce::Colour (0xff14161b));

        const auto& quantizer = processorRef.getQuantizerForDisplay();
        const int numSteps = quantizer.getNumSteps();
        const double loopLength = processorRef.getLoopLengthBeats();
        const int notesPerBeat = processorRef.getNotesPerBeatValue();
        const double beatsPerBar = processorRef.getBeatsPerBar();

        // Horizontal pitch grid - one line per scale step, root rows brighter.
        if (numSteps > 1)
        {
            for (int i = 0; i < numSteps; ++i)
            {
                const float y = bounds.getHeight() * static_cast<float> (i) / static_cast<float> (numSteps - 1);
                const bool root = quantizer.isRootRow (i);
                g.setColour (root ? juce::Colours::white.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.07f));
                g.drawLine (0.0f, y, bounds.getWidth(), y, root ? 1.4f : 1.0f);
            }
        }

        // Vertical time grid - one line per quantize cell, heavier on beats and bars.
        if (loopLength > 0.0 && notesPerBeat > 0)
        {
            const int numCells = juce::jmax (1, static_cast<int> (std::lround (loopLength * notesPerBeat)));
            const double cellsPerBar = beatsPerBar * notesPerBeat;

            for (int i = 0; i <= numCells; ++i)
            {
                const float x = bounds.getWidth() * static_cast<float> (i) / static_cast<float> (numCells);
                const bool barLine = cellsPerBar > 0.0 && std::fmod (static_cast<double> (i), cellsPerBar) < 1.0e-6;
                const bool beatLine = (i % notesPerBeat) == 0;

                float alpha = 0.05f;
                float thickness = 1.0f;
                if (barLine)      { alpha = 0.28f; thickness = 1.6f; }
                else if (beatLine) { alpha = 0.14f; thickness = 1.2f; }

                g.setColour (juce::Colours::white.withAlpha (alpha));
                g.drawLine (x, 0.0f, x, bounds.getHeight(), thickness);
            }
        }

        // Raw strokes: a faint freehand sketch underneath the quantized blocks.
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        for (const auto& stroke : processorRef.getStrokeModel().getStrokes())
        {
            if (stroke.points.size() < 2)
                continue;

            juce::Path path;
            bool first = true;
            for (const auto& point : stroke.points)
            {
                const float x = beatsToX (point.timeBeats, loopLength, bounds.getWidth());
                const float y = point.normalizedY * bounds.getHeight();
                if (first) { path.startNewSubPath (x, y); first = false; }
                else path.lineTo (x, y);
            }
            g.strokePath (path, juce::PathStrokeType (1.5f));
        }

        // Quantized preview: exactly what the NoteTimeline (and therefore the
        // synth/MIDI-out/export) will play.
        if (auto timeline = processorRef.getCurrentTimelineForDisplay())
        {
            const double effectiveLoopLength = timeline->loopLengthBeats > 0.0 ? timeline->loopLengthBeats : loopLength;
            const float rowHeight = numSteps > 1 ? bounds.getHeight() / static_cast<float> (numSteps - 1) : bounds.getHeight();
            const float blockHeight = juce::jmax (4.0f, rowHeight * 0.6f);

            for (const auto& event : timeline->events)
            {
                const float x1 = beatsToX (event.startBeat, effectiveLoopLength, bounds.getWidth());
                const float x2 = beatsToX (event.endBeat, effectiveLoopLength, bounds.getWidth());
                const float yCentre = quantizer.normalizedYForMidiNote (event.midiNote) * bounds.getHeight();

                juce::Rectangle<float> rect (x1 + 1.0f, yCentre - blockHeight * 0.5f,
                                              juce::jmax (2.0f, x2 - x1 - 2.0f), blockHeight);

                g.setColour (juce::Colour (0xff5ec8f2).withAlpha (event.legatoFromPrevious ? 0.65f : 0.9f));
                g.fillRoundedRectangle (rect, 3.0f);
            }
        }

        // Playhead.
        if (processorRef.isTransportPlaying() && loopLength > 0.0)
        {
            const float x = beatsToX (processorRef.getPlayheadBeats(), loopLength, bounds.getWidth());
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawLine (x, 0.0f, x, bounds.getHeight(), 2.0f);
        }

        if (processorRef.getStrokeModel().getStrokes().empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.setFont (juce::Font (juce::FontOptions (16.0f)));
            g.drawText ("Click and drag to draw a melody", getLocalBounds(), juce::Justification::centred);
        }

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRect (getLocalBounds(), 1);
    }
}
