#include "SynthEngine.h"
#include <algorithm>
#include <cmath>

namespace drawsynth
{
    void SynthEngine::prepare (double sampleRate)
    {
        voice.prepare (sampleRate);
    }

    void SynthEngine::reset()
    {
        voice.reset();
        lastSoundingNote = -1;
    }

    void SynthEngine::allNotesOff (juce::MidiBuffer& midiOut, int atSample)
    {
        if (lastSoundingNote != -1)
        {
            voice.stopNote();
            const int channel = juce::jlimit (1, 16, lastSoundingColor + 1);
            midiOut.addEvent (juce::MidiMessage::noteOff (channel, lastSoundingNote, 0.0f), std::max (0, atSample));
            lastSoundingNote = -1;
        }
    }

    void SynthEngine::renderBlock (const NoteTimeline& timeline,
                                    double phaseBeatsAtBlockStart,
                                    double beatsPerSample,
                                    juce::AudioBuffer<float>& audioOut,
                                    juce::MidiBuffer& midiOut)
    {
        const int numSamples = audioOut.getNumSamples();
        if (numSamples <= 0)
            return;

        const double loopLen = timeline.loopLengthBeats;
        if (loopLen <= 0.0 || beatsPerSample <= 0.0)
        {
            // Nothing sensible to schedule; still render whatever the voice
            // is currently doing (e.g. a release tail) so nothing clicks.
            voice.renderAdding (audioOut, 0, numSamples);
            return;
        }

        double phase = std::fmod (phaseBeatsAtBlockStart, loopLen);
        if (phase < 0.0)
            phase += loopLen;

        int destOffset = 0;
        int samplesLeft = numSamples;

        // Guard against pathological inputs (e.g. a huge offline-render block
        // against a tiny loop) looping forever.
        int safetyIterations = numSamples + 4;

        while (samplesLeft > 0 && safetyIterations-- > 0)
        {
            const double beatsToLoopEnd = loopLen - phase;
            int samplesToLoopEnd = static_cast<int> (std::ceil (beatsToLoopEnd / beatsPerSample - 1.0e-9));
            samplesToLoopEnd = std::max (1, samplesToLoopEnd);

            const int segLength = std::min (samplesLeft, samplesToLoopEnd);
            const double segStartBeat = phase;
            const double segEndBeat = phase + segLength * beatsPerSample;

            processSegment (timeline, segStartBeat, segEndBeat, destOffset, segLength, beatsPerSample, audioOut, midiOut);

            destOffset += segLength;
            samplesLeft -= segLength;
            phase += segLength * beatsPerSample;

            if (phase >= loopLen - 1.0e-6)
            {
                // Reached the loop boundary: nothing in a NoteTimeline is
                // ever meant to sustain across the seam, so force silence
                // here rather than risk a hung note.
                allNotesOff (midiOut, destOffset);
                phase = 0.0;
            }
        }
    }

    void SynthEngine::processSegment (const NoteTimeline& timeline,
                                       double segStartBeat, double segEndBeat,
                                       int destOffset, int segLengthSamples,
                                       double beatsPerSample,
                                       juce::AudioBuffer<float>& audioOut,
                                       juce::MidiBuffer& midiOut)
    {
        std::vector<Action> actions;

        for (size_t i = 0; i < timeline.events.size(); ++i)
        {
            const auto& ev = timeline.events[i];

            if (ev.startBeat >= segStartBeat && ev.startBeat < segEndBeat)
                actions.push_back ({ ev.startBeat, true, ev.midiNote, ev.colorIndex, ev.velocity01, ev.legatoFromPrevious });

            // Skip the "end" action when the very next event continues this
            // one seamlessly (legato) - its own "start" action supersedes
            // this note without needing an explicit off first.
            const bool hasSeamlessSuccessor = (i + 1 < timeline.events.size())
                                               && timeline.events[i + 1].legatoFromPrevious
                                               && std::abs (timeline.events[i + 1].startBeat - ev.endBeat) < 1.0e-9;

            if (! hasSeamlessSuccessor && ev.endBeat > segStartBeat && ev.endBeat <= segEndBeat)
                actions.push_back ({ ev.endBeat, false, ev.midiNote, ev.colorIndex, ev.velocity01, false });
        }

        std::sort (actions.begin(), actions.end(), [] (const Action& a, const Action& b)
        {
            if (std::abs (a.beat - b.beat) > 1.0e-9)
                return a.beat < b.beat;
            return (! a.isStart) && b.isStart; // process "end" before "start" at the same instant
        });

        int cursor = 0;

        for (const auto& action : actions)
        {
            int actionSample = static_cast<int> (std::round ((action.beat - segStartBeat) / beatsPerSample));
            actionSample = juce::jlimit (cursor, segLengthSamples, actionSample);

            const int subLen = actionSample - cursor;
            if (subLen > 0)
            {
                voice.renderAdding (audioOut, destOffset + cursor, subLen);
                cursor += subLen;
            }

            const int channel = juce::jlimit (1, 16, action.colorIndex + 1);

            if (action.isStart)
            {
                voice.startNote (action.midiNote, action.velocity01, action.legato);

                if (lastSoundingNote != -1 && lastSoundingNote != action.midiNote)
                {
                    // Same-instant handover, new note-on emitted first: a
                    // mono-legato-aware receiver reads this as a glide;
                    // anything else just sees a clean, gapless handover.
                    midiOut.addEvent (juce::MidiMessage::noteOn (channel, action.midiNote, action.velocity01), destOffset + cursor);
                    midiOut.addEvent (juce::MidiMessage::noteOff (channel, lastSoundingNote, 0.0f), destOffset + cursor);
                }
                else
                {
                    midiOut.addEvent (juce::MidiMessage::noteOn (channel, action.midiNote, action.velocity01), destOffset + cursor);
                }

                lastSoundingNote = action.midiNote;
                lastSoundingColor = action.colorIndex;
            }
            else
            {
                voice.stopNote();
                midiOut.addEvent (juce::MidiMessage::noteOff (channel, action.midiNote, 0.0f), destOffset + cursor);
                lastSoundingNote = -1;
            }
        }

        if (cursor < segLengthSamples)
            voice.renderAdding (audioOut, destOffset + cursor, segLengthSamples - cursor);
    }
}
