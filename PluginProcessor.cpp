#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Canvas/NoteTimelineBuilder.h"
#include "Midi/MidiExporter.h"
#include "State/StrokeSerialization.h"

#include <cmath>

namespace drawsynth
{
    DrawSynthAudioProcessor::DrawSynthAudioProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          parameters (*this, nullptr, juce::Identifier ("PARAMETERS"), createParameterLayout()),
          presetManager (*this)
    {
        rootParam         = parameters.getRawParameterValue (ParamIDs::root);
        scaleParam        = parameters.getRawParameterValue (ParamIDs::scale);
        octaveRangeParam  = parameters.getRawParameterValue (ParamIDs::octaveRange);
        octaveOffsetParam = parameters.getRawParameterValue (ParamIDs::octaveOffset);
        notesPerBeatParam = parameters.getRawParameterValue (ParamIDs::notesPerBeat);
        loopBarsParam     = parameters.getRawParameterValue (ParamIDs::loopBars);
        tempoSyncParam    = parameters.getRawParameterValue (ParamIDs::tempoSync);
        manualBpmParam    = parameters.getRawParameterValue (ParamIDs::manualBpm);
        oscTypeParam      = parameters.getRawParameterValue (ParamIDs::oscType);
        attackParam       = parameters.getRawParameterValue (ParamIDs::attack);
        decayParam        = parameters.getRawParameterValue (ParamIDs::decay);
        sustainParam      = parameters.getRawParameterValue (ParamIDs::sustain);
        releaseParam      = parameters.getRawParameterValue (ParamIDs::release);
        masterGainParam   = parameters.getRawParameterValue (ParamIDs::masterGain);
        playingParam      = parameters.getRawParameterValue (ParamIDs::playing);
        recordingParam    = parameters.getRawParameterValue (ParamIDs::recording);

        lastParamSnapshot.fill (-1);

        startTimerHz (30);
    }

    DrawSynthAudioProcessor::~DrawSynthAudioProcessor()
    {
        stopTimer();
    }

    const juce::String DrawSynthAudioProcessor::getName() const
    {
        return "DrawSynth";
    }

    bool DrawSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto mainOut = layouts.getMainOutputChannelSet();
        return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
    }

    void DrawSynthAudioProcessor::prepareToPlay (double sampleRate, int)
    {
        currentSampleRate = sampleRate;
        synthEngine.prepare (sampleRate);
        phaseBeats = 0.0;
        wasPlayingLastBlock = false;
    }

    void DrawSynthAudioProcessor::releaseResources()
    {
        synthEngine.reset();
    }

    juce::AudioProcessorEditor* DrawSynthAudioProcessor::createEditor()
    {
        return new DrawSynthAudioProcessorEditor (*this);
    }

    //==============================================================================
    void DrawSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return;

        // 1. Host transport / tempo / time signature.
        juce::Optional<juce::AudioPlayHead::PositionInfo> hostPosition;
        if (auto* ph = getPlayHead())
            hostPosition = ph->getPosition();

        bool hostIsPlaying = false;
        double hostBpm = 120.0;
        double hostPpq = 0.0;
        bool hostPpqValid = false;

        if (hostPosition.hasValue())
        {
            hostIsPlaying = hostPosition->getIsPlaying();

            if (auto bpm = hostPosition->getBpm())
                hostBpm = *bpm;

            if (auto sig = hostPosition->getTimeSignature())
                beatsPerBar.store (sig->numerator * 4.0 / static_cast<double> (sig->denominator), std::memory_order_relaxed);

            if (auto ppq = hostPosition->getPpqPosition())
            {
                hostPpq = *ppq;
                hostPpqValid = true;
            }
        }

        const bool syncToHost = *tempoSyncParam >= 0.5f;
        const double bpm = syncToHost ? hostBpm : static_cast<double> (*manualBpmParam);
        lastKnownBpm.store (bpm, std::memory_order_relaxed);

        // 2. Push current synth parameters into the engine (cheap float copies).
        synthEngine.setOscillatorType (static_cast<OscType> (juce::roundToInt ((float) *oscTypeParam)));

        juce::ADSR::Parameters adsrParams;
        adsrParams.attack = *attackParam;
        adsrParams.decay = *decayParam;
        adsrParams.sustain = *sustainParam;
        adsrParams.release = *releaseParam;
        synthEngine.setAdsrParameters (adsrParams);

        const double beatsPerSample = (bpm / 60.0) / currentSampleRate;
        const bool recordingArmed = *recordingParam >= 0.5f;
        const bool isPlaying = *playingParam >= 0.5f;

        // 3. Recording capture happens before we repurpose the MIDI buffer for our own output.
        if (recordingArmed && isPlaying)
            captureIncomingMidiForRecording (midiMessages, phaseBeats, beatsPerSample);

        midiMessages.clear();

        // 4. Thread-safe timeline snapshot.
        std::shared_ptr<const NoteTimeline> timeline;
        {
            const juce::SpinLock::ScopedLockType sl (timelineLock);
            timeline = currentTimeline;
        }

        // 5. Playback.
        if (isPlaying && timeline != nullptr)
        {
            double blockStartPhase = phaseBeats;

            // Lock to the host's own beat clock whenever it's actually
            // running and we're syncing to it, for sample-accurate alignment
            // with the host's bar/beat grid. Otherwise (host stopped, or a
            // detached manual tempo) fall back to our own free-running
            // phase, so the plugin's Play button still works for auditioning
            // with the host transport paused.
            if (syncToHost && hostIsPlaying && hostPpqValid && timeline->loopLengthBeats > 0.0)
            {
                double hostPhase = std::fmod (hostPpq, timeline->loopLengthBeats);
                if (hostPhase < 0.0)
                    hostPhase += timeline->loopLengthBeats;
                blockStartPhase = hostPhase;
            }

            synthEngine.renderBlock (*timeline, blockStartPhase, beatsPerSample, buffer, midiMessages);

            const double loopLen = std::max (0.25, timeline->loopLengthBeats);
            phaseBeats = std::fmod (blockStartPhase + numSamples * beatsPerSample, loopLen);
            if (phaseBeats < 0.0)
                phaseBeats += loopLen;

            wasPlayingLastBlock = true;
        }
        else
        {
            if (wasPlayingLastBlock)
            {
                synthEngine.allNotesOff (midiMessages, 0);
                wasPlayingLastBlock = false;
            }

            // Let any release tail finish naturally instead of hard-cutting.
            synthEngine.renderTailOnly (buffer, 0, numSamples);
        }

        uiPlayheadBeats.store (phaseBeats, std::memory_order_relaxed);

        buffer.applyGain (*masterGainParam);
    }

    void DrawSynthAudioProcessor::captureIncomingMidiForRecording (const juce::MidiBuffer& incoming,
                                                                     double blockStartBeat,
                                                                     double beatsPerSample)
    {
        for (const auto metadata : incoming)
        {
            const auto msg = metadata.getMessage();
            if (! (msg.isNoteOn() || msg.isNoteOff()))
                continue;

            RecordedMidiEvent event;
            event.beatPosition = blockStartBeat + static_cast<double> (metadata.samplePosition) * beatsPerSample;
            event.isNoteOn = msg.isNoteOn();
            event.noteNumber = msg.getNoteNumber();
            event.velocity01 = msg.isNoteOn() ? msg.getFloatVelocity() : 0.0f;

            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            recordFifo.prepareToWrite (1, start1, size1, start2, size2);
            if (size1 == 1)
            {
                recordFifoBuffer[static_cast<size_t> (start1)] = event;
                recordFifo.finishedWrite (1);
            }
            // else: FIFO momentarily full - drop the event rather than block or crash.
        }
    }

    void DrawSynthAudioProcessor::drainRecordingFifo()
    {
        const int numReady = recordFifo.getNumReady();
        if (numReady <= 0)
            return;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        recordFifo.prepareToRead (numReady, start1, size1, start2, size2);

        auto processOne = [this] (const RecordedMidiEvent& event)
        {
            if (event.isNoteOn)
            {
                pendingRecordedNoteStart[event.noteNumber] = event.beatPosition;
                return;
            }

            auto it = pendingRecordedNoteStart.find (event.noteNumber);
            if (it == pendingRecordedNoteStart.end())
                return; // note-off with no matching recorded note-on; ignore

            const double startBeat = it->second;
            pendingRecordedNoteStart.erase (it);

            if (event.beatPosition <= startBeat)
                return; // held across the loop wrap or otherwise degenerate; skip rather than
                        // create a corrupted (negative-length) stroke

            const float y = quantizer.normalizedYForMidiNote (event.noteNumber);
            beginStroke (startBeat, y, 0.8f);
            continueStroke (event.beatPosition, y, 0.8f);
            endStroke();
        };

        for (int i = 0; i < size1; ++i)
            processOne (recordFifoBuffer[static_cast<size_t> (start1 + i)]);
        for (int i = 0; i < size2; ++i)
            processOne (recordFifoBuffer[static_cast<size_t> (start2 + i)]);

        recordFifo.finishedRead (numReady);
    }

    //==============================================================================
    void DrawSynthAudioProcessor::beginStroke (double timeBeats, float normalizedY, float pressure)
    {
        strokeModel.beginStroke (timeBeats, normalizedY, pressure, 0); // v1: single voice/color
        timelineDirty.store (true);
    }

    void DrawSynthAudioProcessor::continueStroke (double timeBeats, float normalizedY, float pressure)
    {
        strokeModel.continueStroke (timeBeats, normalizedY, pressure);
        timelineDirty.store (true);
    }

    void DrawSynthAudioProcessor::endStroke()
    {
        strokeModel.endStroke();
        timelineDirty.store (true);
    }

    void DrawSynthAudioProcessor::clearCanvas()
    {
        strokeModel.clear();
        timelineDirty.store (true);
    }

    void DrawSynthAudioProcessor::undoLastStroke()
    {
        strokeModel.undoLastStroke();
        timelineDirty.store (true);
    }

    void DrawSynthAudioProcessor::eraseStroke (int index)
    {
        if (index < 0)
            return;

        strokeModel.eraseStroke (static_cast<size_t> (index));
        timelineDirty.store (true);
    }

    double DrawSynthAudioProcessor::getLoopLengthBeats() const
    {
        if (auto timeline = getCurrentTimelineForDisplay())
            return timeline->loopLengthBeats;

        const int bars = loopBarsChoiceToValue (juce::roundToInt ((float) *loopBarsParam));
        return bars * beatsPerBar.load (std::memory_order_relaxed);
    }

    int DrawSynthAudioProcessor::getNotesPerBeatValue() const
    {
        return notesPerBeatChoiceToValue (juce::roundToInt ((float) *notesPerBeatParam));
    }

    bool DrawSynthAudioProcessor::isTransportPlaying() const
    {
        return *playingParam >= 0.5f;
    }

    bool DrawSynthAudioProcessor::isRecordingArmed() const
    {
        return *recordingParam >= 0.5f;
    }

    std::shared_ptr<const NoteTimeline> DrawSynthAudioProcessor::getCurrentTimelineForDisplay() const
    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);
        return currentTimeline;
    }

    bool DrawSynthAudioProcessor::exportPatternAsMidiFile (const juce::File& destFile)
    {
        auto timeline = getCurrentTimelineForDisplay();
        if (timeline == nullptr)
            return false;

        return MidiExporter::exportToFile (*timeline, lastKnownBpm.load (std::memory_order_relaxed), destFile);
    }

    //==============================================================================
    void DrawSynthAudioProcessor::syncQuantizerFromParameters()
    {
        quantizer.setRoot (juce::roundToInt ((float) *rootParam));
        quantizer.setScale (static_cast<ScaleType> (juce::roundToInt ((float) *scaleParam)));
        quantizer.setOctaveRange (juce::roundToInt ((float) *octaveRangeParam));
        quantizer.setOctaveOffset (juce::roundToInt ((float) *octaveOffsetParam));
    }

    void DrawSynthAudioProcessor::publishTimeline (std::shared_ptr<const NoteTimeline> newTimeline)
    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);
        currentTimeline = std::move (newTimeline);
    }

    void DrawSynthAudioProcessor::rebuildTimelineIfNeeded()
    {
        if (! timelineDirty.exchange (false))
            return;

        syncQuantizerFromParameters();

        const int bars = loopBarsChoiceToValue (juce::roundToInt ((float) *loopBarsParam));
        const double loopLength = bars * beatsPerBar.load (std::memory_order_relaxed);
        const int notesPerBeatValue = notesPerBeatChoiceToValue (juce::roundToInt ((float) *notesPerBeatParam));

        auto newTimeline = std::make_shared<NoteTimeline> (
            NoteTimelineBuilder::build (strokeModel.getStrokes(), quantizer, loopLength, notesPerBeatValue));

        publishTimeline (std::move (newTimeline));
    }

    void DrawSynthAudioProcessor::timerCallback()
    {
        // Poll timeline-affecting parameters for changes. Polling (rather
        // than an AudioProcessorValueTreeState::Listener callback) is used
        // deliberately: host automation can change these from the audio
        // thread mid-block, and polling on the message thread sidesteps any
        // question of which thread a listener callback would fire on.
        const std::array<int, 6> snapshot {
            juce::roundToInt ((float) *rootParam), juce::roundToInt ((float) *scaleParam),
            juce::roundToInt ((float) *octaveRangeParam), juce::roundToInt ((float) *octaveOffsetParam),
            juce::roundToInt ((float) *notesPerBeatParam), juce::roundToInt ((float) *loopBarsParam)
        };

        if (snapshot != lastParamSnapshot)
        {
            lastParamSnapshot = snapshot;
            timelineDirty.store (true);
        }

        static double lastSeenBeatsPerBar = -1.0;
        const double currentBeatsPerBar = beatsPerBar.load (std::memory_order_relaxed);
        if (std::abs (currentBeatsPerBar - lastSeenBeatsPerBar) > 1.0e-9)
        {
            lastSeenBeatsPerBar = currentBeatsPerBar;
            timelineDirty.store (true);
        }

        rebuildTimelineIfNeeded();
        drainRecordingFifo();
    }

    //==============================================================================
    void DrawSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        auto state = parameters.copyState();
        state.appendChild (StrokeSerialization::toValueTree (strokeModel), nullptr);

        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void DrawSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
        if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
            return;

        auto newState = juce::ValueTree::fromXml (*xml);
        auto strokesTree = newState.getChildWithName ("STROKES");

        parameters.replaceState (newState);

        StrokeSerialization::restoreFromValueTree (strokeModel, strokesTree);
        timelineDirty.store (true);
    }
}

//==============================================================================
// Required by every JUCE plugin format wrapper to obtain the AudioProcessor instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new drawsynth::DrawSynthAudioProcessor();
}
