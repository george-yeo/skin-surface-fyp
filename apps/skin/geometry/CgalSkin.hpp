#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "skin/geometry/SkinMesh.hpp"
#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

// CGAL's Skin_surface_3 mesh of the balls (grow_balls off, so the same skin
// as the mixed complex's), refined by `subdivisions` rounds of CGAL's
// subdivision. Vertex normals are area-weighted; triangles are untagged.
SkinMesh meshSkinWithCgal(std::span<const WeightedPoint> points, double s, int subdivisions);

struct SkinComparison {
    size_t oursVertices = 0;
    size_t oursTriangles = 0;
    size_t cgalVertices = 0;
    size_t cgalTriangles = 0;
    size_t located = 0; // CGAL vertices found inside some mixed cell
    // |F| / |grad F| at CGAL's vertices: first-order distance to our skin.
    double maxResidual = 0.0;
    double meanResidual = 0.0;
    // Vertex-to-mesh distances, both ways.
    double maxCgalToOurs = 0.0;
    double meanCgalToOurs = 0.0;
    double maxOursToCgal = 0.0;
    double meanOursToCgal = 0.0;
};

SkinComparison compareSkins(const SkinMesh& ours, const SkinMesh& cgal,
                            const MixedComplexTopology& topology,
                            const std::vector<SkinQuadric>& quadrics);

} // namespace skin::geometry
