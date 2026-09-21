#include "NoteTimelineBuilder.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace drawsynth
{
    namespace
    {
        // Internal time resolution used to resolve *what actually plays*,
        // independent of the user-facing Quantize setting (which only
        // controls where note boundaries get snapped for a clean rhythmic
        // feel - see placeBoundaryTime). 480 divisions per beat mirrors a common
        // MIDI PPQ resolution: far finer than any realistic mouse gesture,
        // so it never itself becomes the bottleneck that loses a fast pitch
        // sweep, and fine enough that a self-crossing stroke's overwrite
        // resolution (see below) lands right where the pen actually was.
        constexpr int kFineDivisionsPerBeat = 480;

        struct FineCell
        {
            bool hasNote = false;
            int step = -1;
            float velocity01 = 0.8f;
        };

        struct RawNote { double startBeat; double endBeat; int step; float velocity01; };

        // Places a note boundary's time: snapped to the quantize grid ONLY
        // when the boundary already sits close to a grid line (within
        // kSnapToleranceFraction of one cell) *and* snapping leaves room on
        // both sides (doesn't collide with the previous placement or eat
        // into the next boundary's own raw position); otherwise the exact
        // (unsnapped) time is used unmodified. This is deliberately
        // conservative: always-round-to-nearest sounds fine in isolation,
        // but when several raw boundaries from a fast run land close
        // together under a coarse grid, forcing every one of them toward the
        // same grid line creates a cascade of collisions that previously
        // degenerated into a string of near-zero-length notes. Restricting
        // snapping to boundaries that are already near a grid line - i.e.
        // slow, deliberate drawing - avoids that case structurally: a dense
        // run's boundaries are scattered through a beat by construction, so
        // essentially none of them qualify, and they simply keep their
        // exact (already correctly ordered) timing instead.
        double placeBoundaryTime (double exactTime, double previousPlacedTime, double nextRawTime, int notesPerBeat)
        {
            constexpr double kTinyEpsilon = 1.0e-6;
            constexpr double kSnapToleranceFraction = 0.15;

            const double gridCell = 1.0 / static_cast<double> (notesPerBeat);
            const double nearestGridLine = std::round (exactTime * notesPerBeat) / static_cast<double> (notesPerBeat);
            const double distanceToGridLine = std::abs (exactTime - nearestGridLine);

            const bool closeToGrid = distanceToGridLine <= kSnapToleranceFraction * gridCell;
            const bool clearsPrevious = nearestGridLine > previousPlacedTime + kTinyEpsilon;
            const bool clearsNext = nearestGridLine < nextRawTime - kTinyEpsilon;

            double candidate = (closeToGrid && clearsPrevious && clearsNext) ? nearestGridLine : exactTime;

            if (candidate <= previousPlacedTime + kTinyEpsilon)
                candidate = previousPlacedTime + kTinyEpsilon;

            return candidate;
        }

        // Builds one stroke's own run of NoteEvents (its own monophonic
        // melodic lane) and appends them to the output timeline.
        void buildLaneForStroke (const Stroke& stroke, int laneId, const ScaleQuantizer& quantizer,
                                  double loopLengthBeats, int userNotesPerBeat, std::vector<NoteEvent>& outEvents)
        {
            if (stroke.points.empty())
                return;

            std::vector<NoteEvent> laneEvents;

            if (stroke.points.size() == 1)
            {
                // A click with no drag: one note, one grid cell long, snapped
                // to the cell it falls in.
                const double t = stroke.points.front().timeBeats;
                const double cellStart = std::floor (t * userNotesPerBeat) / userNotesPerBeat;

                NoteEvent ev;
                ev.startBeat = cellStart;
                ev.endBeat = cellStart + 1.0 / userNotesPerBeat;
                ev.midiNote = quantizer.quantizeNormalizedYToMidiNote (stroke.points.front().normalizedY);
                ev.colorIndex = stroke.colorIndex;
                ev.laneId = laneId;
                ev.velocity01 = stroke.points.front().pressure;
                ev.legatoFromPrevious = false;
                laneEvents.push_back (ev);
            }
            else
            {
                // IMPORTANT: points are walked in the order they were
                // actually drawn (chronological/recording order), NOT sorted
                // by time/X position. A stroke that crosses back over an
                // earlier X position (a loop, a scribble, a "come back and
                // fix this bit" gesture) needs the *later* part of the
                // gesture to win at that position - sorting by X would
                // instead interleave two physically distant parts of the
                // path that happen to share an X value, scrambling the
                // result. Painting onto a fine grid in drawing order gives
                // exactly "later ink overwrites earlier ink", the same
                // principle already used between separate overlapping
                // strokes, just applied within one stroke's own self-overlaps.
                const int numFineCells = std::max (1, static_cast<int> (
                    std::llround (std::max (0.25, loopLengthBeats) * kFineDivisionsPerBeat)));
                std::vector<FineCell> fineCells (static_cast<size_t> (numFineCells));

                for (size_t i = 1; i < stroke.points.size(); ++i)
                {
                    const StrokePoint& p1 = stroke.points[i - 1];
                    const StrokePoint& p2 = stroke.points[i];

                    const double tLo = std::min (p1.timeBeats, p2.timeBeats);
                    const double tHi = std::max (p1.timeBeats, p2.timeBeats);

                    int cellLo = static_cast<int> (std::floor (tLo * kFineDivisionsPerBeat));
                    int cellHi = static_cast<int> (std::floor (tHi * kFineDivisionsPerBeat));
                    cellLo = std::clamp (cellLo, 0, numFineCells - 1);
                    cellHi = std::clamp (cellHi, 0, numFineCells - 1);

                    const double dt = p2.timeBeats - p1.timeBeats;

                    for (int c = cellLo; c <= cellHi; ++c)
                    {
                        const double cellCentre = (static_cast<double> (c) + 0.5) / kFineDivisionsPerBeat;
                        double alpha = (std::abs (dt) > 1.0e-9) ? (cellCentre - p1.timeBeats) / dt : 0.0;
                        alpha = std::clamp (alpha, 0.0, 1.0);

                        const float y = static_cast<float> (p1.normalizedY + alpha * (p2.normalizedY - p1.normalizedY));
                        const float pressure = static_cast<float> (p1.pressure + alpha * (p2.pressure - p1.pressure));

                        FineCell& cell = fineCells[static_cast<size_t> (c)];
                        cell.hasNote = true;
                        cell.step = quantizer.quantizeNormalizedYToStep (y);
                        cell.velocity01 = pressure;
                    }
                }

                // Safety net: guarantee every fine cell within the stroke's
                // own drawn time range ends up painted. The chain of
                // pairwise-touching segments above should mathematically
                // leave no gap on its own, but "wherever the pen was, there
                // must be a note" is exactly what a drawing instrument
                // promises, so this is enforced directly and unconditionally
                // rather than trusted to hold implicitly: any cell still
                // unpainted within [minCell, maxCell] is filled from its
                // nearest painted neighbour (forward pass, then a backward
                // pass for a leading gap before the very first painted cell).
                {
                    double minTime = stroke.points.front().timeBeats;
                    double maxTime = stroke.points.front().timeBeats;
                    for (const auto& p : stroke.points)
                    {
                        minTime = std::min (minTime, p.timeBeats);
                        maxTime = std::max (maxTime, p.timeBeats);
                    }

                    const int minCell = std::clamp (static_cast<int> (std::floor (minTime * kFineDivisionsPerBeat)), 0, numFineCells - 1);
                    const int maxCell = std::clamp (static_cast<int> (std::floor (maxTime * kFineDivisionsPerBeat)), 0, numFineCells - 1);

                    int lastPainted = -1;
                    for (int c = minCell; c <= maxCell; ++c)
                    {
                        if (fineCells[static_cast<size_t> (c)].hasNote)
                            lastPainted = c;
                        else if (lastPainted != -1)
                            fineCells[static_cast<size_t> (c)] = fineCells[static_cast<size_t> (lastPainted)];
                    }

                    int nextPainted = -1;
                    for (int c = maxCell; c >= minCell; --c)
                    {
                        if (fineCells[static_cast<size_t> (c)].hasNote)
                            nextPainted = c;
                        else if (nextPainted != -1)
                            fineCells[static_cast<size_t> (c)] = fineCells[static_cast<size_t> (nextPainted)];
                    }
                }

                // Run-length encode the painted fine grid into raw, unsnapped notes.
                std::vector<RawNote> rawNotes;
                int i = 0;
                while (i < numFineCells)
                {
                    if (! fineCells[static_cast<size_t> (i)].hasNote) { ++i; continue; }

                    const int startCell = i;
                    const int step = fineCells[static_cast<size_t> (i)].step;
                    int j = i + 1;
                    while (j < numFineCells && fineCells[static_cast<size_t> (j)].hasNote
                           && fineCells[static_cast<size_t> (j)].step == step)
                        ++j;

                    rawNotes.push_back ({ static_cast<double> (startCell) / kFineDivisionsPerBeat,
                                           static_cast<double> (j) / kFineDivisionsPerBeat,
                                           step, fineCells[static_cast<size_t> (startCell)].velocity01 });
                    i = j;
                }

                // Snap boundaries to the user-facing Quantize grid. Two raw
                // notes that are seamless (no gap) share one "anchor" time so
                // they still snap to exactly the same instant on both sides -
                // otherwise independent snapping could reopen a gap (or close
                // one that should stay open) between two notes that used to
                // meet exactly.
                std::vector<double> anchorRawTimes;
                std::vector<int> startAnchor (rawNotes.size());
                std::vector<int> endAnchor (rawNotes.size());

                for (size_t k = 0; k < rawNotes.size(); ++k)
                {
                    if (k > 0 && std::abs (rawNotes[k].startBeat - rawNotes[k - 1].endBeat) < 1.0e-9)
                        startAnchor[k] = endAnchor[k - 1];
                    else
                    {
                        anchorRawTimes.push_back (rawNotes[k].startBeat);
                        startAnchor[k] = static_cast<int> (anchorRawTimes.size()) - 1;
                    }

                    anchorRawTimes.push_back (rawNotes[k].endBeat);
                    endAnchor[k] = static_cast<int> (anchorRawTimes.size()) - 1;
                }

                std::vector<double> placedAnchorTimes (anchorRawTimes.size());
                double previousPlaced = -std::numeric_limits<double>::infinity();
                for (size_t a = 0; a < anchorRawTimes.size(); ++a)
                {
                    const double nextRawTime = (a + 1 < anchorRawTimes.size())
                                                ? anchorRawTimes[a + 1]
                                                : anchorRawTimes[a] + (1.0 / userNotesPerBeat);
                    placedAnchorTimes[a] = placeBoundaryTime (anchorRawTimes[a], previousPlaced, nextRawTime, userNotesPerBeat);
                    previousPlaced = placedAnchorTimes[a];
                }

                for (size_t k = 0; k < rawNotes.size(); ++k)
                {
                    NoteEvent ev;
                    ev.startBeat = placedAnchorTimes[static_cast<size_t> (startAnchor[k])];
                    ev.endBeat = placedAnchorTimes[static_cast<size_t> (endAnchor[k])];
                    ev.midiNote = quantizer.getMidiNoteForStep (rawNotes[k].step);
                    ev.colorIndex = stroke.colorIndex;
                    ev.laneId = laneId;
                    ev.velocity01 = rawNotes[k].velocity01;
                    ev.legatoFromPrevious = (k > 0) && (startAnchor[k] == endAnchor[k - 1]);
                    laneEvents.push_back (ev);
                }
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

        for (size_t strokeIdx = 0; strokeIdx < strokes.size(); ++strokeIdx)
            buildLaneForStroke (strokes[strokeIdx], static_cast<int> (strokeIdx), quantizer,
                                 timeline.loopLengthBeats, safeNotesPerBeat, timeline.events);

        // Lanes were appended one stroke at a time, so the combined list
        // needs re-sorting into overall chronological order. A stable sort
        // keeps each lane's own events in their original relative order.
        std::stable_sort (timeline.events.begin(), timeline.events.end(),
                           [] (const NoteEvent& a, const NoteEvent& b) { return a.startBeat < b.startBeat; });

        return timeline;
    }
}
