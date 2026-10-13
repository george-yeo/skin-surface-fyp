#pragma once

namespace skin::vis {

struct UserSettings {
    bool showWeights = false;
    bool wireframe = false;

    // Visualisation layers
    bool showDelaunayFaces = true;
    bool showDelaunayEdges = true;
    bool showVoronoiEdges = true;
    bool showDelaunayTets = false;  // shrunk towards each tet's centroid
    bool showVoronoiCells = false;  // clipped to the skin box, shrunk towards each centroid
    bool showSelectedCell = false;  // one vertex's cell + its dual Delaunay edges
    bool showClipBox = false;       // skin box B and the larger display box
    float skinBoxPadding = 0.5f;    // B's padding, in ball-bounds diagonals
    float displayBoxPadding = 1.0f; // D's padding (ray stub length), in B diagonals
    int selectedVertex = 0;         // index into PowerComplex::vertices()
    float tetShrink = 0.8f;         // purely visual: 1 = true size
    float cellShrink = 0.85f;       // purely visual: 1 = true size

    // Mesh layer opacity; below 1 the layer draws in the transparent pass.
    float delaunayFaceAlpha = 1.0f;
    float tetAlpha = 1.0f;
    float cellAlpha = 1.0f;
    float selectedCellAlpha = 1.0f;
    float mixedAlpha = 1.0f;

    bool showMixedCells = false;
    float mixedS = 0.5f;   // skin shrink factor s
    float mixedGap = 1.0f; // purely visual: 1 = true geometry
    bool showMixedVertexCells = true;
    bool showMixedEdgeCells = true;
    bool showMixedTriangleCells = true;
    bool showMixedTetCells = true;

    bool operator==(const UserSettings&) const = default;
};

} // namespace skin::vis
