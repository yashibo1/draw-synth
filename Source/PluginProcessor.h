#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Music/ScaleQuantizer.h"
#include "Canvas/StrokeModel.h"
#include "Canvas/NoteTimeline.h"
#include "Audio/SynthEngine.h"

#include <array>
#include <atomic>
#include <map>
#include <memory>

namespace drawsynth
{
    struct RecordedMidiEvent
    {
        double beatPosition = 0.0;
        bool isNoteOn = false;
        int noteNumber = 0;
        float velocity01 = 0.8f;
    };

    class DrawSynthAudioProcessor : public juce::AudioProcessor,
                                     private juce::Timer
    {
    public:
        DrawSynthAudioProcessor();
        ~DrawSynthAudioProcessor() override;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        using AudioProcessor::processBlock; // this plugin only supports float processing
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override;
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 3.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        // --- API used by the editor / canvas UI (message-thread only) ---
        void beginStroke (double timeBeats, float normalizedY, float pressure);
        void continueStroke (double timeBeats, float normalizedY, float pressure);
        void endStroke();
        void clearCanvas();
        void undoLastStroke();

        double getLoopLengthBeats() const;
        int getNotesPerBeatValue() const;
        double getBeatsPerBar() const noexcept { return beatsPerBar.load (std::memory_order_relaxed); }

        double getPlayheadBeats() const noexcept { return uiPlayheadBeats.load (std::memory_order_relaxed); }
        bool isTransportPlaying() const;
        bool isRecordingArmed() const;
        double getLastKnownBpm() const noexcept { return lastKnownBpm.load (std::memory_order_relaxed); }

        std::shared_ptr<const NoteTimeline> getCurrentTimelineForDisplay() const;
        const StrokeModel& getStrokeModel() const noexcept { return strokeModel; }
        const ScaleQuantizer& getQuantizerForDisplay() const noexcept { return quantizer; }

        bool exportPatternAsMidiFile (const juce::File& destFile);

        juce::AudioProcessorValueTreeState parameters;

    private:
        void timerCallback() override;
        void rebuildTimelineIfNeeded();
        void syncQuantizerFromParameters();
        void publishTimeline (std::shared_ptr<const NoteTimeline> newTimeline);
        void captureIncomingMidiForRecording (const juce::MidiBuffer& incoming, double blockStartBeat, double beatsPerSample);
        void drainRecordingFifo();

        // --- Canvas / music model (message-thread only) ---
        StrokeModel strokeModel;
        ScaleQuantizer quantizer;
        std::atomic<bool> timelineDirty { true };
        std::atomic<double> beatsPerBar { 4.0 };
        std::array<int, 6> lastParamSnapshot {};

        // --- Thread-safe published timeline ---
        // A SpinLock-guarded shared_ptr swap: the message thread rebuilds and
        // publishes a whole new NoteTimeline on stroke/parameter changes; the
        // audio thread takes a cheap copy of the pointer each block. This is
        // "realtime-safe enough" rather than textbook wait-free: in the rare
        // case the audio thread ends up holding the last reference to an old
        // timeline when its local copy is destroyed, that small vector's
        // deallocation happens on the audio thread. Given timelines are only
        // republished on user edits (not every block) and are small, this is
        // a deliberate, documented trade-off rather than an oversight - see
        // the README for the lock-free alternative if you need harder
        // real-time guarantees.
        mutable juce::SpinLock timelineLock;
        std::shared_ptr<const NoteTimeline> currentTimeline;

        // --- Audio-thread transport state (owned exclusively by processBlock) ---
        double phaseBeats = 0.0;
        bool wasPlayingLastBlock = false;
        std::atomic<double> uiPlayheadBeats { 0.0 };
        std::atomic<double> lastKnownBpm { 120.0 };

        // --- Cached raw parameter pointers (thread-safe atomics owned by APVTS) ---
        std::atomic<float>* rootParam = nullptr;
        std::atomic<float>* scaleParam = nullptr;
        std::atomic<float>* octaveRangeParam = nullptr;
        std::atomic<float>* octaveOffsetParam = nullptr;
        std::atomic<float>* notesPerBeatParam = nullptr;
        std::atomic<float>* loopBarsParam = nullptr;
        std::atomic<float>* tempoSyncParam = nullptr;
        std::atomic<float>* manualBpmParam = nullptr;
        std::atomic<float>* oscTypeParam = nullptr;
        std::atomic<float>* attackParam = nullptr;
        std::atomic<float>* decayParam = nullptr;
        std::atomic<float>* sustainParam = nullptr;
        std::atomic<float>* releaseParam = nullptr;
        std::atomic<float>* masterGainParam = nullptr;
        std::atomic<float>* playingParam = nullptr;
        std::atomic<float>* recordingParam = nullptr;

        // --- Recording: incoming MIDI -> canvas strokes ---
        // Audio thread writes, message thread (timer) drains. AbstractFifo
        // handles the cross-thread synchronisation; everything downstream of
        // the drain (including pendingRecordedNoteStart) is message-thread-only.
        juce::AbstractFifo recordFifo { 512 };
        std::array<RecordedMidiEvent, 512> recordFifoBuffer;
        std::map<int, double> pendingRecordedNoteStart;

        SynthEngine synthEngine;
        double currentSampleRate = 44100.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawSynthAudioProcessor)
    };
}
