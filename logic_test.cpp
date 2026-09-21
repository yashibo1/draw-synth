// Standalone sanity test for the pure-logic core (Scale, ScaleQuantizer,
// StrokeModel, NoteTimelineBuilder). Deliberately has NO JUCE dependency so
// it compiles and runs in seconds with a plain compiler:
//
//   g++ -std=c++17 -I../Source tests/logic_test.cpp
//       Source/Music/Scale.cpp Source/Music/ScaleQuantizer.cpp
//       Source/Canvas/StrokeModel.cpp Source/Canvas/NoteTimelineBuilder.cpp
//       -o logic_test && ./logic_test
//
#include "../Source/Music/Scale.h"
#include "../Source/Music/ScaleQuantizer.h"
#include "../Source/Canvas/StrokeModel.h"
#include "../Source/Canvas/NoteTimelineBuilder.h"

#include <cstdio>
#include <cmath>

using namespace drawsynth;

namespace
{
    int failures = 0;
    int checks = 0;

    void expect (bool condition, const char* description)
    {
        ++checks;
        if (! condition)
        {
            ++failures;
            std::printf ("  [FAIL] %s\n", description);
        }
    }

    void section (const char* name)
    {
        std::printf ("-- %s --\n", name);
    }
}

int main()
{
    section ("ScaleQuantizer basic mapping (C Major, 1 octave)");
    {
        ScaleQuantizer q;
        q.setRoot (0);                 // C
        q.setScale (ScaleType::Major);
        q.setOctaveRange (1);
        q.setOctaveOffset (0);

        expect (q.getNumSteps() == 8, "1 octave major scale has 8 grid steps (7 degrees + top root)");
        expect (q.isRootRow (0), "top row is a root row (window closes on the root)");
        expect (q.isRootRow (7), "bottom row is a root row");
        expect (! q.isRootRow (1), "second-from-top row is not a root row");

        const int top = q.getMidiNoteForStep (0);
        const int bottom = q.getMidiNoteForStep (7);
        expect (top - bottom == 12, "top and bottom of a 1-octave window are exactly 12 semitones apart");

        expect (q.quantizeNormalizedYToStep (0.0f) == 0, "normalizedY=0 quantizes to the top step");
        expect (q.quantizeNormalizedYToStep (1.0f) == 7, "normalizedY=1 quantizes to the bottom step");

        // Inverse mapping should round-trip.
        const int midStep = 3;
        const int midNote = q.getMidiNoteForStep (midStep);
        const float y = q.normalizedYForMidiNote (midNote);
        expect (q.quantizeNormalizedYToStep (y) == midStep, "normalizedYForMidiNote round-trips through quantizeNormalizedYToStep");
    }

    section ("Octave range affects step count, not just span");
    {
        ScaleQuantizer q;
        q.setScale (ScaleType::Major);
        q.setOctaveRange (2);
        expect (q.getNumSteps() == 15, "2 octave major scale has 15 grid steps (7*2 + 1)");
    }

    section ("NoteTimelineBuilder: continuous ascending stroke");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (1);

        StrokeModel model;
        // One continuous drag from bottom-left to top-right across a 4 beat loop.
        model.beginStroke (0.0, 1.0f, 0.8f, 0);
        model.continueStroke (1.0, 0.75f, 0.8f);
        model.continueStroke (2.0, 0.5f, 0.8f);
        model.continueStroke (3.0, 0.25f, 0.8f);
        model.continueStroke (4.0, 0.0f, 0.8f);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);

        expect (! timeline.events.empty(), "ascending stroke produces at least one note event");

        bool nonDecreasing = true;
        for (size_t i = 1; i < timeline.events.size(); ++i)
            if (timeline.events[i].midiNote < timeline.events[i - 1].midiNote)
                nonDecreasing = false;
        expect (nonDecreasing, "pitch never decreases along a monotonically rising stroke");

        bool allLegatoAfterFirst = true;
        for (size_t i = 1; i < timeline.events.size(); ++i)
            if (! timeline.events[i].legatoFromPrevious)
                allLegatoAfterFirst = false;
        expect (! timeline.events.empty() && ! timeline.events.front().legatoFromPrevious,
                "first event of a stroke is never legato (nothing to glide from)");
        expect (allLegatoAfterFirst, "every subsequent event within one continuous stroke is legato-linked");

        bool allButLastHaveSuccessor = true;
        for (size_t i = 0; i + 1 < timeline.events.size(); ++i)
            if (! timeline.events[i].hasSeamlessSuccessor)
                allButLastHaveSuccessor = false;
        expect (allButLastHaveSuccessor, "every event but the last has hasSeamlessSuccessor set, matching the legato chain");
        expect (! timeline.events.empty() && ! timeline.events.back().hasSeamlessSuccessor,
                "the last event in a lane has nothing following it, so hasSeamlessSuccessor is false");
    }

    section ("NoteTimelineBuilder: two separate strokes with a rest between them");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (1);

        StrokeModel model;
        model.beginStroke (0.0, 0.8f, 0.8f, 0);
        model.continueStroke (1.0, 0.8f, 0.8f);
        model.endStroke();

        // Deliberate gap: nothing drawn between beat 1 and beat 3.

        model.beginStroke (3.0, 0.2f, 0.8f, 0);
        model.continueStroke (4.0, 0.2f, 0.8f);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);

        expect (timeline.events.size() == 2, "two separate strokes with a rest produce exactly two events");
        if (timeline.events.size() == 2)
        {
            expect (! timeline.events[1].legatoFromPrevious, "a new stroke after a rest is never legato");
            expect (timeline.events[0].endBeat <= timeline.events[1].startBeat,
                    "events from separate strokes never overlap");
        }
    }

    section ("NoteTimelineBuilder: a click with no drag produces one short note");
    {
        ScaleQuantizer q;
        StrokeModel model;
        model.beginStroke (2.0, 0.5f, 0.8f, 0);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);
        expect (timeline.events.size() == 1, "a single click produces exactly one note event");
        if (! timeline.events.empty())
            expect (timeline.events[0].endBeat - timeline.events[0].startBeat > 0.0,
                    "the clicked note has non-zero duration (one grid cell)");
    }

    section ("Quantize-on-the-fly: changing key/scale re-maps existing strokes without touching them");
    {
        StrokeModel model;
        model.beginStroke (0.0, 0.5f, 0.8f, 0);
        model.continueStroke (2.0, 0.5f, 0.8f);
        model.endStroke();

        ScaleQuantizer cMajor;
        cMajor.setRoot (0);
        cMajor.setScale (ScaleType::Major);
        const auto timelineCMajor = NoteTimelineBuilder::build (model.getStrokes(), cMajor, 4.0, 1);

        ScaleQuantizer aMinor;
        aMinor.setRoot (9);
        aMinor.setScale (ScaleType::NaturalMinor);
        const auto timelineAMinor = NoteTimelineBuilder::build (model.getStrokes(), aMinor, 4.0, 1);

        expect (! model.getStrokes().empty(), "strokes are untouched by re-quantization (sanity)");
        expect (! timelineCMajor.events.empty() && ! timelineAMinor.events.empty(),
                "both quantizations produce notes from the same raw stroke");
        if (! timelineCMajor.events.empty() && ! timelineAMinor.events.empty())
            expect (timelineCMajor.events[0].midiNote != timelineAMinor.events[0].midiNote,
                    "the same raw stroke resolves to a different note under a different key/scale");
    }

    section ("StrokeModel: clear and undo");
    {
        StrokeModel model;
        model.beginStroke (0.0, 0.5f, 0.8f, 0);
        model.endStroke();
        model.beginStroke (1.0, 0.5f, 0.8f, 0);
        model.endStroke();
        expect (model.getStrokes().size() == 2, "two strokes recorded");

        model.undoLastStroke();
        expect (model.getStrokes().size() == 1, "undo removes exactly the last stroke");

        model.clear();
        expect (model.isEmpty(), "clear empties the model");
    }

    section ("NoteTimelineBuilder: overlapping strokes form a chord instead of overwriting each other");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (2);

        StrokeModel model;
        // A low, long-held note...
        model.beginStroke (0.0, 0.9f, 0.8f, 0);
        model.continueStroke (4.0, 0.9f, 0.8f);
        model.endStroke();
        // ...and a high note drawn on top of the same time range, as a second stroke.
        model.beginStroke (0.0, 0.1f, 0.8f, 0);
        model.continueStroke (4.0, 0.1f, 0.8f);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);

        expect (timeline.events.size() == 2, "two overlapping strokes produce two simultaneous events, not one replacing the other");
        if (timeline.events.size() == 2)
        {
            expect (timeline.events[0].laneId != timeline.events[1].laneId, "overlapping notes are on different lanes");
            expect (timeline.events[0].midiNote != timeline.events[1].midiNote, "the low and high strokes keep their own distinct pitches (a chord)");
        }
    }

    section ("NoteTimelineBuilder: a steep, fast stroke produces multiple notes, not one");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (2); // 15 grid steps

        StrokeModel model;
        // Simulates a fast near-vertical drag: lots of Y movement packed
        // into a tiny time window, as mouseDrag would actually record it.
        model.beginStroke (1.00, 1.0f, 0.8f, 0);
        model.continueStroke (1.01, 0.8f, 0.8f);
        model.continueStroke (1.02, 0.6f, 0.8f);
        model.continueStroke (1.03, 0.4f, 0.8f);
        model.continueStroke (1.04, 0.2f, 0.8f);
        model.continueStroke (1.05, 0.0f, 0.8f);
        model.endStroke();

        // notesPerBeat=1 means the whole gesture above sits inside a single
        // grid cell - exactly the case that used to collapse to one note.
        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);

        expect (timeline.events.size() > 1,
                "a steep stroke confined to a fraction of one grid cell still yields more than one note");

        bool strictlyOrderedAndAscendingPitch = true;
        for (size_t i = 1; i < timeline.events.size(); ++i)
        {
            if (timeline.events[i].startBeat <= timeline.events[i - 1].startBeat)
                strictlyOrderedAndAscendingPitch = false;
            if (timeline.events[i].midiNote <= timeline.events[i - 1].midiNote)
                strictlyOrderedAndAscendingPitch = false;
        }
        expect (strictlyOrderedAndAscendingPitch, "the fast run's notes are in strictly increasing time and pitch order");

        bool allWithinGrabbedWindow = true;
        for (const auto& ev : timeline.events)
            if (ev.startBeat < 0.9 || ev.startBeat > 1.2)
                allWithinGrabbedWindow = false;
        expect (allWithinGrabbedWindow, "the fast run's notes stay close to where it was actually drawn, not smeared elsewhere");
    }

    section ("NoteTimelineBuilder: a self-crossing stroke lets the later-drawn part win, not a sort-order scramble");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (2); // 15 steps - plenty of room to tell "near top" and "near bottom" apart

        StrokeModel model;
        // Draw left-to-right along the top (high pitch)...
        model.beginStroke (0.0, 0.1f, 0.8f, 0);
        model.continueStroke (2.0, 0.1f, 0.8f);
        // ...then, in the SAME stroke, sweep back from right to left down to
        // the bottom (low pitch) - crossing back over the same time range
        // the first leg already covered, like tracing a loop.
        model.continueStroke (0.0, 0.9f, 0.8f);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);

        expect (! timeline.events.empty(), "a self-crossing stroke still produces notes");

        const NoteEvent* noteAtStart = nullptr;
        for (const auto& ev : timeline.events)
        {
            if (ev.startBeat <= 0.05 && ev.endBeat > 0.0)
            {
                noteAtStart = &ev;
                break;
            }
        }

        expect (noteAtStart != nullptr, "there is a note covering the very start of the stroke");
        if (noteAtStart != nullptr)
        {
            const int earlierPitch = q.quantizeNormalizedYToMidiNote (0.1f); // what the first (overwritten) leg alone implied
            const int laterPitch = q.quantizeNormalizedYToMidiNote (0.9f);   // what the second (later-drawn) leg implied
            expect (noteAtStart->midiNote != earlierPitch,
                    "the note at the start is NOT the pitch the first-drawn leg alone would give");
            expect (noteAtStart->midiNote == laterPitch,
                    "the note at the start matches the leg drawn later, i.e. later ink wins");
        }

        // The second leg starts at the same point (t=2, y=0.1) the first leg
        // ends at, so the very last note in this single-lane timeline (the
        // one actually reaching t=2) should agree with both legs there - a
        // useful check that the handoff between legs isn't corrupted.
        expect (! timeline.events.empty(), "there is at least one note to check at the shared boundary");
        if (! timeline.events.empty())
        {
            const auto& lastNote = timeline.events.back();
            const int sharedPitch = q.quantizeNormalizedYToMidiNote (0.1f);
            expect (std::abs (lastNote.endBeat - 2.0) < 0.01, "the final note in this stroke ends right at the shared boundary point");
            expect (lastNote.midiNote == sharedPitch, "the final note matches what both legs agree on at the shared boundary");
        }
    }

    section ("NoteTimelineBuilder: a coarse grid never produces a cascade of degenerate near-zero-length notes");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (3); // 22 steps - lots of pitch content to pack into a short span

        StrokeModel model;
        // A deliberately messy multi-reversal gesture (like tracing a rough
        // loop by hand) under the coarsest possible Quantize setting (1/beat),
        // which is exactly the combination that used to cascade into a string
        // of near-instantaneous notes right after each successful grid snap.
        model.beginStroke (0.00, 0.95f, 0.8f, 0);
        model.continueStroke (1.90, 0.05f, 0.8f);
        model.continueStroke (0.10, 0.85f, 0.8f);
        model.continueStroke (1.95, 0.15f, 0.8f);
        model.continueStroke (0.05, 0.75f, 0.8f);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 4.0, 1);

        constexpr double kSaneMinimumDuration = 1.0 / 480.0; // one fine-grid cell; anything shorter is a placement bug
        bool noDegenerateNotes = true;
        for (const auto& ev : timeline.events)
            if (ev.endBeat - ev.startBeat < kSaneMinimumDuration - 1.0e-9)
                noDegenerateNotes = false;

        expect (noDegenerateNotes, "even a messy, reversal-heavy gesture under a coarse grid produces no near-zero-length notes");
    }

    section ("NoteTimelineBuilder: a full loop shape (top, left descent, bottom, big right arc back) has no gaps");
    {
        ScaleQuantizer q;
        q.setRoot (0);
        q.setScale (ScaleType::Major);
        q.setOctaveRange (2);

        StrokeModel model;
        // Mirrors the reported shape: start at the top, descend on the left,
        // trace the bottom, then a wide arc back up the right side closing
        // near the top again - the exact topology that was losing notes.
        model.beginStroke (6.0, 0.15f, 0.8f, 0);
        model.continueStroke (6.2, 0.30f, 0.8f);
        model.continueStroke (6.3, 0.45f, 0.8f);
        model.continueStroke (6.3, 0.60f, 0.8f);
        model.continueStroke (2.3, 0.50f, 0.8f);
        model.continueStroke (1.5, 0.75f, 0.8f);
        model.continueStroke (1.7, 0.85f, 0.8f);
        model.continueStroke (2.5, 0.95f, 0.8f);
        model.continueStroke (4.5, 0.95f, 0.8f);
        model.continueStroke (6.5, 0.70f, 0.8f);
        model.continueStroke (7.5, 0.45f, 0.8f);
        model.continueStroke (8.0, 0.25f, 0.8f);
        model.continueStroke (6.5, 0.15f, 0.8f);
        model.endStroke();

        const auto timeline = NoteTimelineBuilder::build (model.getStrokes(), q, 16.0, 2);

        expect (! timeline.events.empty(), "the full loop shape produces notes");

        bool noGaps = true;
        for (size_t i = 1; i < timeline.events.size(); ++i)
            if (timeline.events[i].startBeat - timeline.events[i - 1].endBeat > 1.0e-6)
                noGaps = false;
        expect (noGaps, "there is no silent gap anywhere between the first and last note of this shape");

        // The whole drawn extent runs from t=1.5 (leftmost) to t=8.0 (the arc's peak).
        expect (timeline.events.front().startBeat <= 1.55, "coverage starts at the leftmost point actually drawn");
        expect (timeline.events.back().endBeat >= 7.95, "coverage reaches the rightmost point actually drawn (the arc's peak)");
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
