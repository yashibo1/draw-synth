#include "StrokeSerialization.h"

namespace drawsynth
{
    namespace StrokeSerialization
    {
        juce::ValueTree toValueTree (const StrokeModel& model)
        {
            juce::ValueTree root ("STROKES");

            for (const auto& stroke : model.getStrokes())
            {
                juce::ValueTree strokeTree ("STROKE");
                strokeTree.setProperty ("color", stroke.colorIndex, nullptr);

                for (const auto& point : stroke.points)
                {
                    juce::ValueTree pointTree ("POINT");
                    pointTree.setProperty ("t", point.timeBeats, nullptr);
                    pointTree.setProperty ("y", static_cast<double> (point.normalizedY), nullptr);
                    pointTree.setProperty ("p", static_cast<double> (point.pressure), nullptr);
                    strokeTree.appendChild (pointTree, nullptr);
                }

                root.appendChild (strokeTree, nullptr);
            }

            return root;
        }

        void restoreFromValueTree (StrokeModel& model, const juce::ValueTree& tree)
        {
            model.clear();

            if (! tree.isValid() || tree.getType().toString() != "STROKES")
                return;

            for (int s = 0; s < tree.getNumChildren(); ++s)
            {
                auto strokeTree = tree.getChild (s);
                if (strokeTree.getNumChildren() == 0)
                    continue;

                const int colorIndex = static_cast<int> (strokeTree.getProperty ("color", 0));
                bool first = true;

                for (int p = 0; p < strokeTree.getNumChildren(); ++p)
                {
                    auto pointTree = strokeTree.getChild (p);
                    const double t = static_cast<double> (pointTree.getProperty ("t", 0.0));
                    const float y = static_cast<float> (static_cast<double> (pointTree.getProperty ("y", 0.5)));
                    const float pressure = static_cast<float> (static_cast<double> (pointTree.getProperty ("p", 0.8)));

                    if (first)
                    {
                        model.beginStroke (t, y, pressure, colorIndex);
                        first = false;
                    }
                    else
                    {
                        model.continueStroke (t, y, pressure);
                    }
                }

                model.endStroke();
            }
        }
    }
}
