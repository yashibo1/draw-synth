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

        // What (if anything) is sounding at one grid cell, after every stroke
        // has had a chance to "paint" over it. Newer strokes overwrite older
        // ones, matching the visual expectation that later ink sits on top.
        struct CellPaint
        {
            bool hasNote = false;
            int midiNote = -1;
            int strokeIndex = -1;
            int colorIndex = 0;
            float velocity01 = 0.8f;
        };
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

        std::vector<CellPaint> cells (static_cast<size_t> (numCells));

        for (size_t strokeIdx = 0; strokeIdx < strokes.size(); ++strokeIdx)
        {
            const Stroke& stroke = strokes[strokeIdx];
            if (stroke.points.empty())
                continue;

            std::vector<StrokePoint> sorted = stroke.points;
            std::stable_sort (sorted.begin(), sorted.end(),
                               [] (const StrokePoint& a, const StrokePoint& b) { return a.timeBeats < b.timeBeats; });

            // A click with no drag: light up just the one cell it falls in.
            if (sorted.size() == 1)
            {
                const double t = sorted.front().timeBeats;
                int cellIndex = static_cast<int> (std::floor (t * safeNotesPerBeat));
                cellIndex = ((cellIndex % numCells) + numCells) % numCells;

                CellPaint& cell = cells[static_cast<size_t> (cellIndex)];
                cell.hasNote = true;
                cell.midiNote = quantizer.quantizeNormalizedYToMidiNote (sorted.front().normalizedY);
                cell.strokeIndex = static_cast<int> (strokeIdx);
                cell.colorIndex = stroke.colorIndex;
                cell.velocity01 = sorted.front().pressure;
                continue;
            }

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
                cell.strokeIndex = static_cast<int> (strokeIdx);
                cell.colorIndex = stroke.colorIndex;
                cell.velocity01 = interp.pressure;
            }
        }

        // Run-length encode the composited grid into NoteEvents. Consecutive
        // cells with the same pitch AND the same source stroke merge into one
        // sustained event; a pitch change with the same source stroke is
        // marked legato (continuous pen gesture -> glide, no re-attack); a
        // change of source stroke (or a gap) always starts fresh.
        int i = 0;
        while (i < numCells)
        {
            if (! cells[static_cast<size_t> (i)].hasNote)
            {
                ++i;
                continue;
            }

            const int startCell = i;
            const CellPaint& first = cells[static_cast<size_t> (i)];
            const int note = first.midiNote;
            const int strokeIdx = first.strokeIndex;

            int j = i + 1;
            while (j < numCells)
            {
                const CellPaint& c = cells[static_cast<size_t> (j)];
                if (! c.hasNote || c.midiNote != note || c.strokeIndex != strokeIdx)
                    break;
                ++j;
            }

            NoteEvent ev;
            ev.startBeat = static_cast<double> (startCell) / safeNotesPerBeat;
            ev.endBeat = static_cast<double> (j) / safeNotesPerBeat;
            ev.midiNote = note;
            ev.colorIndex = first.colorIndex;
            ev.velocity01 = first.velocity01;
            ev.legatoFromPrevious = startCell > 0
                                     && cells[static_cast<size_t> (startCell - 1)].hasNote
                                     && cells[static_cast<size_t> (startCell - 1)].strokeIndex == strokeIdx;

            timeline.events.push_back (ev);
            i = j;
        }

        return timeline;
    }
}
