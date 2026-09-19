#include "MidiExporter.h"
#include <cmath>

namespace drawsynth
{
    namespace MidiExporter
    {
        bool exportToFile (const NoteTimeline& timeline, double bpm, const juce::File& destFile)
        {
            constexpr int ticksPerQuarterNote = 960;
            const double safeBpm = bpm > 0.0 ? bpm : 120.0;

            juce::MidiMessageSequence sequence;

            const int microsecondsPerQuarterNote = static_cast<int> (std::round (60000000.0 / safeBpm));
            sequence.addEvent (juce::MidiMessage::tempoMetaEvent (microsecondsPerQuarterNote), 0.0);
            sequence.addEvent (juce::MidiMessage::textMetaEvent (3, "DrawSynth Pattern"), 0.0);

            for (const auto& ev : timeline.events)
            {
                const double startTick = ev.startBeat * ticksPerQuarterNote;
                const double endTick = ev.endBeat * ticksPerQuarterNote;
                const int channel = juce::jlimit (1, 16, ev.colorIndex + 1);

                sequence.addEvent (juce::MidiMessage::noteOn (channel, ev.midiNote, ev.velocity01), startTick);
                sequence.addEvent (juce::MidiMessage::noteOff (channel, ev.midiNote, 0.0f), endTick);
            }

            sequence.updateMatchedPairs();
            sequence.sort();

            juce::MidiFile midiFile;
            midiFile.setTicksPerQuarterNote (ticksPerQuarterNote);
            midiFile.addTrack (sequence);

            if (! destFile.getParentDirectory().exists())
                destFile.getParentDirectory().createDirectory();

            juce::TemporaryFile tempFile (destFile);
            {
                juce::FileOutputStream stream (tempFile.getFile());
                if (! stream.openedOk())
                    return false;

                if (! midiFile.writeTo (stream))
                    return false;
            }

            return tempFile.overwriteTargetFileWithTemporary();
        }
    }
}
