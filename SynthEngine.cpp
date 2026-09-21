#include "SynthEngine.h"
#include <algorithm>
#include <cmath>

namespace drawsynth
{
    void SynthEngine::prepare (double sampleRate)
    {
        for (auto& slot : voiceSlots)
            slot.voice.prepare (sampleRate);
    }

    void SynthEngine::reset()
    {
        for (auto& slot : voiceSlots)
        {
            slot.voice.reset();
            slot.assignedLaneId = -1;
            slot.currentMidiNote = -1;
            slot.currentColorIndex = 0;
        }
        laneToVoiceSlot.clear();
        triggerCounter = 0;
    }

    void SynthEngine::setOscillatorType (OscType type)
    {
        for (auto& slot : voiceSlots)
            slot.voice.setOscillatorType (type);
    }

    void SynthEngine::setAdsrParameters (const juce::ADSR::Parameters& params)
    {
        for (auto& slot : voiceSlots)
            slot.voice.setAdsrParameters (params);
    }

    void SynthEngine::allNotesOff (juce::MidiBuffer& midiOut, int atSample)
    {
        const int clampedSample = std::max (0, atSample);

        for (auto& slot : voiceSlots)
        {
            if (slot.currentMidiNote != -1)
            {
                slot.voice.stopNote();
                const int channel = juce::jlimit (1, 16, slot.currentColorIndex + 1);
                midiOut.addEvent (juce::MidiMessage::noteOff (channel, slot.currentMidiNote, 0.0f), clampedSample);
                slot.currentMidiNote = -1;
            }
            slot.assignedLaneId = -1;
        }

        laneToVoiceSlot.clear();
    }

    void SynthEngine::renderTailOnly (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        for (auto& slot : voiceSlots)
            slot.voice.renderAdding (buffer, startSample, numSamples);
    }

    int SynthEngine::findOrAllocateVoiceForLane (int laneId, bool preferExistingLaneVoice)
    {
        if (preferExistingLaneVoice)
        {
            auto it = laneToVoiceSlot.find (laneId);
            if (it != laneToVoiceSlot.end())
                return it->second;
            // No existing voice for this lane (shouldn't normally happen for
            // a legato event, but fall through to a fresh allocation rather
            // than dropping the note if it ever does).
        }

        for (int i = 0; i < kMaxVoices; ++i)
        {
            if (voiceSlots[static_cast<size_t> (i)].assignedLaneId == -1)
            {
                voiceSlots[static_cast<size_t> (i)].assignedLaneId = laneId;
                laneToVoiceSlot[laneId] = i;
                return i;
            }
        }

        // Every voice is busy: steal the least-recently-triggered one.
        int stealIndex = 0;
        uint64_t oldestOrder = voiceSlots[0].triggerOrder;
        for (int i = 1; i < kMaxVoices; ++i)
        {
            if (voiceSlots[static_cast<size_t> (i)].triggerOrder < oldestOrder)
            {
                oldestOrder = voiceSlots[static_cast<size_t> (i)].triggerOrder;
                stealIndex = i;
            }
        }

        const int previousLane = voiceSlots[static_cast<size_t> (stealIndex)].assignedLaneId;
        if (previousLane != -1)
            laneToVoiceSlot.erase (previousLane);

        voiceSlots[static_cast<size_t> (stealIndex)].assignedLaneId = laneId;
        laneToVoiceSlot[laneId] = stealIndex;
        return stealIndex;
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
            renderTailOnly (audioOut, 0, numSamples);
            return;
        }

        double phase = std::fmod (phaseBeatsAtBlockStart, loopLen);
        if (phase < 0.0)
            phase += loopLen;

        int destOffset = 0;
        int samplesLeft = numSamples;
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
                // Nothing in a NoteTimeline is ever meant to sustain across
                // the loop seam - force everything off here.
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

        for (const auto& ev : timeline.events)
        {
            if (ev.startBeat >= segStartBeat && ev.startBeat < segEndBeat)
                actions.push_back ({ ev.startBeat, true, ev.midiNote, ev.colorIndex, ev.laneId, ev.velocity01, ev.legatoFromPrevious });

            if (! ev.hasSeamlessSuccessor && ev.endBeat > segStartBeat && ev.endBeat <= segEndBeat)
                actions.push_back ({ ev.endBeat, false, ev.midiNote, ev.colorIndex, ev.laneId, ev.velocity01, false });
        }

        std::stable_sort (actions.begin(), actions.end(), [] (const Action& a, const Action& b)
        {
            if (a.beat != b.beat)
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
                for (auto& slot : voiceSlots)
                    slot.voice.renderAdding (audioOut, destOffset + cursor, subLen);
                cursor += subLen;
            }

            const int channel = juce::jlimit (1, 16, action.colorIndex + 1);

            if (action.isStart)
            {
                const int slotIndex = findOrAllocateVoiceForLane (action.laneId, action.legato);
                auto& slot = voiceSlots[static_cast<size_t> (slotIndex)];

                const bool trueLegato = action.legato && slot.currentMidiNote != -1;
                slot.voice.startNote (action.midiNote, action.velocity01, trueLegato);
                slot.triggerOrder = ++triggerCounter;
                slot.currentColorIndex = action.colorIndex;

                if (trueLegato && slot.currentMidiNote != action.midiNote)
                {
                    // Same-instant handover on the same voice, new note-on
                    // emitted first: a mono-legato-aware receiver reads this
                    // as a glide; anything else just sees a gapless handover.
                    midiOut.addEvent (juce::MidiMessage::noteOn (channel, action.midiNote, action.velocity01), destOffset + cursor);
                    midiOut.addEvent (juce::MidiMessage::noteOff (channel, slot.currentMidiNote, 0.0f), destOffset + cursor);
                }
                else
                {
                    midiOut.addEvent (juce::MidiMessage::noteOn (channel, action.midiNote, action.velocity01), destOffset + cursor);
                }

                slot.currentMidiNote = action.midiNote;
            }
            else
            {
                auto it = laneToVoiceSlot.find (action.laneId);
                if (it != laneToVoiceSlot.end())
                {
                    auto& slot = voiceSlots[static_cast<size_t> (it->second)];
                    slot.voice.stopNote();
                    if (slot.currentMidiNote != -1)
                        midiOut.addEvent (juce::MidiMessage::noteOff (channel, slot.currentMidiNote, 0.0f), destOffset + cursor);
                    slot.currentMidiNote = -1;
                    slot.assignedLaneId = -1;
                    laneToVoiceSlot.erase (it);
                }
            }
        }

        if (cursor < segLengthSamples)
            for (auto& slot : voiceSlots)
                slot.voice.renderAdding (audioOut, destOffset + cursor, segLengthSamples - cursor);
    }
}
