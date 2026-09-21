#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Canvas/StrokeModel.h"

namespace drawsynth
{
    // Converts StrokeModel <-> juce::ValueTree so the drawn pattern can be
    // saved/restored as part of the plugin's project state. Kept separate
    // from StrokeModel itself so the core data model has no JUCE dependency
    // and can be unit-tested with a plain compiler (see tests/logic_test.cpp).
    namespace StrokeSerialization
    {
        juce::ValueTree toValueTree (const StrokeModel& model);
        void restoreFromValueTree (StrokeModel& model, const juce::ValueTree& tree);
    }
}
