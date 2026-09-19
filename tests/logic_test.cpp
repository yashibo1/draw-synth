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

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
