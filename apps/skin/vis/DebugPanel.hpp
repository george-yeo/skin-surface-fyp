#pragma once

#include <optional>

#include "skin/geometry/fwd.hpp"
#include "skin/vis/UserSettings.hpp"
#include "skin/vis/fwd.hpp"

namespace skin::vis {

// The "Settings" window. Call from inside an ImGui frame.
class DebugPanel {
    public:
    // Returns the edited settings if anything changed this frame.
    std::optional<UserSettings> draw(const UserSettings& current,
                                     const geometry::ComplexModel& model,
                                     const MixedCellCounts& mixedCounts);

    private:
    // Slider values while dragging; only committed to UserSettings on release
    // (each commit rebuilds every tet/cell mesh).
    float tetShrinkDraft = 0.8f;
    float cellShrinkDraft = 0.85f;
    float mixedSDraft = 0.5f;
    float mixedGapDraft = 1.0f;
    float delaunayFaceAlphaDraft = 1.0f;
    float tetAlphaDraft = 1.0f;
    float cellAlphaDraft = 1.0f;
    float selectedCellAlphaDraft = 1.0f;
    float mixedAlphaDraft = 1.0f;
    float skinBoxPaddingDraft = 0.5f;
    float displayBoxPaddingDraft = 1.0f;
};

} // namespace skin::vis
