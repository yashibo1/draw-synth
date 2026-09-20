#include "NoteTimelineBuilder.h"
#include <algorithm>
#include <cmath>
#include <limits>

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

        // One point where the stroke's quantized pitch changes: "starting at
        // this time, the pitch is this step". Built by walking the stroke's
        // own recorded points pairwise and finding every scale-step boundary
        // crossed between each consecutive pair - not just sampling once per
        // grid cell. This is what makes a fast, steep drag (a lot of pitch
        // change packed into very little time, e.g. dragging straight down)
        // produce one note per pitch instead of collapsing to a single note:
        // a time-grid-only sampler only takes one snapshot per cell and can
        // miss everything else the stroke passed through inside that cell.
        struct StepBreakpoint { double time; int step; };

        std::vector<StepBreakpoint> computeStepBreakpoints (const std::vector<StrokePoint>& sortedPoints,
                                                              const ScaleQuantizer& quantizer)
        {
            std::vector<StepBreakpoint> breakpoints;
            if (sortedPoints.empty())
                return breakpoints;

            const int numSteps = quantizer.getNumSteps();
            int currentStep = quantizer.quantizeNormalizedYToStep (sortedPoints.front().normalizedY);
            breakpoints.push_back ({ sortedPoints.front().timeBeats, currentStep });

            for (size_t i = 1; i < sortedPoints.size(); ++i)
            {
                const StrokePoint& p1 = sortedPoints[i - 1];
                const StrokePoint& p2 = sortedPoints[i];
                const int step2 = quantizer.quantizeNormalizedYToStep (p2.normalizedY);

                if (step2 == currentStep)
                    continue;

                const int direction = step2 > currentStep ? 1 : -1;
                const double y1 = p1.normalizedY;
                const double y2 = p2.normalizedY;

                while (currentStep != step2)
                {
                    const int nextStep = currentStep + direction;
                    double crossingTime = p2.timeBeats;

                    if (numSteps > 1 && std::abs (y2 - y1) > 1.0e-9)
                    {
                        // The boundary between two adjacent steps sits halfway
                        // between them in normalizedY (quantization rounds to
                        // the nearest step), so solve for where the straight
                        // line from p1 to p2 crosses that halfway point.
                        const double boundaryStepValue = (static_cast<double> (currentStep) + static_cast<double> (nextStep)) * 0.5;
                        const double yBoundary = boundaryStepValue / static_cast<double> (numSteps - 1);
                        double alpha = (yBoundary - y1) / (y2 - y1);
                        alpha = std::clamp (alpha, 0.0, 1.0);
                        crossingTime = p1.timeBeats + alpha * (p2.timeBeats - p1.timeBeats);
                    }

                    breakpoints.push_back ({ crossingTime, nextStep });
                    currentStep = nextStep;
                }
            }

            return breakpoints;
        }

        // Places a breakpoint's note-start time: snapped to the quantize grid
        // when that's unambiguous, falling back to the breakpoint's exact
        // (unsnapped) time when snapping would collide with or reorder past
        // the previous note, and finally forcing a tiny forward nudge in the
        // rare case even the exact time doesn't clear the previous one (e.g.
        // the previous note's snap rounded forward past it). This keeps
        // ordinary slow drawing snapped cleanly to the grid while never
        // dropping or reordering notes from a fast run that the grid is too
        // coarse to hold on its own.
        double placeNextTime (double exactTime, double previousPlacedTime, int notesPerBeat)
        {
            constexpr double kTinyEpsilon = 1.0e-6;

            const double snapped = std::round (exactTime * notesPerBeat) / static_cast<double> (notesPerBeat);
            double candidate = (snapped > previousPlacedTime + kTinyEpsilon) ? snapped : exactTime;

            if (candidate <= previousPlacedTime + kTinyEpsilon)
                candidate = previousPlacedTime + kTinyEpsilon;

            return candidate;
        }

        // Builds one stroke's own run of NoteEvents (its own monophonic
        // melodic lane) and appends them to the output timeline.
        void buildLaneForStroke (const Stroke& stroke, int laneId, const ScaleQuantizer& quantizer,
                                  int safeNotesPerBeat, std::vector<NoteEvent>& outEvents)
        {
            if (stroke.points.empty())
                return;

            std::vector<StrokePoint> sorted = stroke.points;
            std::stable_sort (sorted.begin(), sorted.end(),
                               [] (const StrokePoint& a, const StrokePoint& b) { return a.timeBeats < b.timeBeats; });

            std::vector<NoteEvent> laneEvents;

            if (sorted.size() == 1)
            {
                // A click with no drag: one note, one grid cell long, snapped
                // to the cell it falls in (nothing to detect a run in here).
                const double t = sorted.front().timeBeats;
                const double cellStart = std::floor (t * safeNotesPerBeat) / safeNotesPerBeat;

                NoteEvent ev;
                ev.startBeat = cellStart;
                ev.endBeat = cellStart + 1.0 / safeNotesPerBeat;
                ev.midiNote = quantizer.quantizeNormalizedYToMidiNote (sorted.front().normalizedY);
                ev.colorIndex = stroke.colorIndex;
                ev.laneId = laneId;
                ev.velocity01 = sorted.front().pressure;
                ev.legatoFromPrevious = false;
                laneEvents.push_back (ev);
            }
            else
            {
                const auto breakpoints = computeStepBreakpoints (sorted, quantizer);

                std::vector<double> placedTimes (breakpoints.size());
                double previousPlaced = -std::numeric_limits<double>::infinity();
                for (size_t i = 0; i < breakpoints.size(); ++i)
                {
                    placedTimes[i] = placeNextTime (breakpoints[i].time, previousPlaced, safeNotesPerBeat);
                    previousPlaced = placedTimes[i];
                }

                const double strokeEndExact = sorted.back().timeBeats;
                const double strokeEndPlaced = placeNextTime (strokeEndExact, previousPlaced, safeNotesPerBeat);

                for (size_t i = 0; i < breakpoints.size(); ++i)
                {
                    NoteEvent ev;
                    ev.startBeat = placedTimes[i];
                    ev.endBeat = (i + 1 < breakpoints.size()) ? placedTimes[i + 1] : strokeEndPlaced;
                    ev.midiNote = quantizer.getMidiNoteForStep (breakpoints[i].step);
                    ev.colorIndex = stroke.colorIndex;
                    ev.laneId = laneId;
                    ev.velocity01 = interpolateAt (sorted, breakpoints[i].time).pressure;
                    ev.legatoFromPrevious = (i > 0);
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
                                 safeNotesPerBeat, timeline.events);

        // Lanes were appended one stroke at a time, so the combined list
        // needs re-sorting into overall chronological order. A stable sort
        // keeps each lane's own events in their original relative order.
        std::stable_sort (timeline.events.begin(), timeline.events.end(),
                           [] (const NoteEvent& a, const NoteEvent& b) { return a.startBeat < b.startBeat; });

        return timeline;
    }
}
