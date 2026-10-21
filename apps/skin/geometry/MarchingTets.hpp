#pragma once

#include <cstddef>
#include <vector>

#include "skin/geometry/SkinMesh.hpp"
#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

struct MarchingTetsResult {
    SkinMesh mesh;
    size_t tets = 0;             // after refinement
    bool budgetExceeded = false; // refinement stopped early
    bool cancelled = false;      // stop requested; the mesh is incomplete
};

// Marching tetrahedra over the mixed complex, each cell with its own quadric.
// Cells are coned from their centroid over faces fanned from their lowest
// welded id, so neighbours triangulate shared walls identically. Tets are
// refined by longest-edge bisection, propagated to every tet sharing the
// edge so the mesh stays conforming, while their longest edge exceeds
// k * the cell's patch radius and they may meet the skin. F is evaluated once
// per vertex and each crossing solved once per edge, so the mesh is
// watertight across walls. With a control, reports progress and returns
// early (cancelled) once a stop is requested.
MarchingTetsResult marchTets(const MixedComplex& mixed, const MixedComplexTopology& topology,
                             const std::vector<SkinQuadric>& quadrics, double k,
                             MeshingControl* control = nullptr);

} // namespace skin::geometry
