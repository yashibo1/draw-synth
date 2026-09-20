#include "NoteTimelineBuilder.h"
#include <algorithm>
#include <cmath>

namespace drawsynth
{
    namespace
    {
        struct InterpResult { float y; float pressure; };

        // Linearly interpolates Y/pressure at time t within a single stroke's
        // sorted points. Clamps to the stroke's ends rather than extrapolating.
        InterpResult interpolateAt (const std::vector<StrokePoint>& sortedPoints, double t)
        {
            if (sortedPoints.size() == 1)
                return { sortedPoints.front().normalizedY, sortedPoints.front().pressure };

            if (t <= sortedPoints.front().timeBeats)
                return { sortedPoints.front().normalizedY, sortedPoints.front().pressure };

            if (t >= sortedPoints.back().timeBeats)
                return { sortedPoints.back().normalizedY, sortedPoints.back().pressure };

            auto it = std::lower_bound (sortedPoints.begin(), sortedPoints.end(), t,
                                         [] (const StrokePoint& p, double time) { return p.timeBeats < time; });

            const StrokePoint& after = *it;
            const StrokePoint& before = *(it - 1);

            const double span = after.timeBeats - before.timeBeats;
            const double alpha = span > 1.0e-9 ? (t - before.timeBeats) / span : 0.0;

            const float y = static_cast<float> (before.normalizedY + alpha * (after.normalizedY - before.normalizedY));
            const float pressure = static_cast<float> (before.pressure + alpha * (after.pressure - before.pressure));
            return { y, pressure };
        }

        // What (if anything) is sounding at one grid cell, for a *single*
        // stroke's own cell array. Each stroke gets an independent array -
        // strokes are never composited against each other, which is exactly
        // what lets two overlapping strokes coexist as a chord.
        struct CellPaint
        {
            bool hasNote = false;
            int midiNote = -1;
            float velocity01 = 0.8f;
        };

        // Builds one stroke's own run of NoteEvents (its own monophonic
        // melodic lane) and appends them to the output timeline.
        void buildLaneForStroke (const Stroke& stroke, int laneId, const ScaleQuantizer& quantizer,
                                  int numCells, int safeNotesPerBeat, std::vector<NoteEvent>& outEvents)
        {
            if (stroke.points.empty())
                return;

            std::vector<StrokePoint> sorted = stroke.points;
            std::stable_sort (sorted.begin(), sorted.end(),
                               [] (const StrokePoint& a, const StrokePoint& b) { return a.timeBeats < b.timeBeats; });

            std::vector<CellPaint> cells (static_cast<size_t> (numCells));

            // A click with no drag: light up just the one cell it falls in.
            if (sorted.size() == 1)
            {
                const double t = sorted.front().timeBeats;
                int cellIndex = static_cast<int> (std::floor (t * safeNotesPerBeat));
                cellIndex = ((cellIndex % numCells) + numCells) % numCells;

                CellPaint& cell = cells[static_cast<size_t> (cellIndex)];
                cell.hasNote = true;
                cell.midiNote = quantizer.quantizeNormalizedYToMidiNote (sorted.front().normalizedY);
                cell.velocity01 = sorted.front().pressure;
            }
            else
            {
                const double minT = sorted.front().timeBeats;
                const double maxT = sorted.back().timeBeats;

                for (int i = 0; i < numCells; ++i)
                {
                    const double cellCentre = (static_cast<double> (i) + 0.5) / safeNotesPerBeat;
                    if (cellCentre < minT || cellCentre > maxT)
                        continue;

                    const auto interp = interpolateAt (sorted, cellCentre);
                    CellPaint& cell = cells[static_cast<size_t> (i)];
                    cell.hasNote = true;
                    cell.midiNote = quantizer.quantizeNormalizedYToMidiNote (interp.y);
                    cell.velocity01 = interp.pressure;
                }
            }

            // Run-length encode this one stroke's cells into NoteEvents.
            // Consecutive same-pitch cells merge into one sustained event; a
            // pitch change is marked legato (continuous pen gesture -> glide,
            // no re-attack); a gap always starts fresh.
            std::vector<NoteEvent> laneEvents;
            int i = 0;
            while (i < numCells)
            {
                if (! cells[static_cast<size_t> (i)].hasNote)
                {
                    ++i;
                    continue;
                }

                const int startCell = i;
                const int note = cells[static_cast<size_t> (i)].midiNote;

                int j = i + 1;
                while (j < numCells && cells[static_cast<size_t> (j)].hasNote && cells[static_cast<size_t> (j)].midiNote == note)
                    ++j;

                NoteEvent ev;
                ev.startBeat = static_cast<double> (startCell) / safeNotesPerBeat;
                ev.endBeat = static_cast<double> (j) / safeNotesPerBeat;
                ev.midiNote = note;
                ev.colorIndex = stroke.colorIndex;
                ev.laneId = laneId;
                ev.velocity01 = cells[static_cast<size_t> (startCell)].velocity01;
                ev.legatoFromPrevious = startCell > 0 && cells[static_cast<size_t> (startCell - 1)].hasNote;

                laneEvents.push_back (ev);
                i = j;
            }

            // Now that this lane's own event order is known, mark each event
            // that is immediately, seamlessly superseded by the next one.
            for (size_t k = 1; k < laneEvents.size(); ++k)
                if (laneEvents[k].legatoFromPrevious)
                    laneEvents[k - 1].hasSeamlessSuccessor = true;

            outEvents.insert (outEvents.end(), laneEvents.begin(), laneEvents.end());
        }
    }

    NoteTimeline NoteTimelineBuilder::build (const std::vector<Stroke>& strokes,
                                              const ScaleQuantizer& quantizer,
                                              double loopLengthBeats,
                                              int notesPerBeat)
    {
        NoteTimeline timeline;
        timeline.loopLengthBeats = std::max (0.25, loopLengthBeats);

        const int safeNotesPerBeat = std::max (1, notesPerBeat);
        const int numCells = std::max (1, static_cast<int> (std::lround (timeline.loopLengthBeats * safeNotesPerBeat)));

        for (size_t strokeIdx = 0; strokeIdx < strokes.size(); ++strokeIdx)
            buildLaneForStroke (strokes[strokeIdx], static_cast<int> (strokeIdx), quantizer,
                                 numCells, safeNotesPerBeat, timeline.events);

        // Lanes were appended one stroke at a time, so the combined list
        // needs re-sorting into overall chronological order. A stable sort
        // keeps each lane's own events in their original relative order.
        std::stable_sort (timeline.events.begin(), timeline.events.end(),
                           [] (const NoteEvent& a, const NoteEvent& b) { return a.startBeat < b.startBeat; });

        return timeline;
    }
}
