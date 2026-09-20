#include "StrokeModel.h"

namespace drawsynth
{
    void StrokeModel::beginStroke (double timeBeats, float normalizedY, float pressure, int colorIndex)
    {
        Stroke s;
        s.colorIndex = colorIndex;
        s.points.push_back ({ timeBeats, normalizedY, pressure });
        strokes.push_back (std::move (s));
        activeStrokeIndex = static_cast<int> (strokes.size()) - 1;
    }

    void StrokeModel::continueStroke (double timeBeats, float normalizedY, float pressure)
    {
        if (activeStrokeIndex < 0 || activeStrokeIndex >= static_cast<int> (strokes.size()))
            return;

        strokes[static_cast<size_t> (activeStrokeIndex)].points.push_back ({ timeBeats, normalizedY, pressure });
    }

    void StrokeModel::endStroke()
    {
        // Drop degenerate zero-length strokes (a click with no drag produces
        // exactly one point, which is valid - a single short note - so only
        // discard truly empty strokes here, which shouldn't normally occur).
        if (activeStrokeIndex >= 0 && activeStrokeIndex < static_cast<int> (strokes.size())
            && strokes[static_cast<size_t> (activeStrokeIndex)].points.empty())
        {
            strokes.erase (strokes.begin() + activeStrokeIndex);
        }

        activeStrokeIndex = -1;
    }

    void StrokeModel::clear()
    {
        strokes.clear();
        activeStrokeIndex = -1;
    }

    void StrokeModel::undoLastStroke()
    {
        if (! strokes.empty())
            strokes.pop_back();

        activeStrokeIndex = -1;
    }

    void StrokeModel::eraseStroke (size_t index)
    {
        if (index < strokes.size())
            strokes.erase (strokes.begin() + static_cast<std::ptrdiff_t> (index));

        activeStrokeIndex = -1;
    }
}
