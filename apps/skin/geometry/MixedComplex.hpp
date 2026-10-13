#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

// mu_sigma = (1 - s) * sigma + s * nu_sigma for a Delaunay simplex sigma and
// its Voronoi dual nu_sigma, as a convex polytope given by its boundary.
struct MixedCell {
    uint32_t dimension; // of the Delaunay simplex, 0..3
    uint32_t simplex;   // index into vertices()/edges()/triangles()/tets()
    std::vector<std::vector<glm::dvec3>> faces; // convex polygons, cyclic order
};

struct MixedComplex {
    std::vector<MixedCell> cells;
    std::array<uint32_t, 4> clipped{}; // kept cells cut by the box, by simplex dimension
};

// Mixed cells intersected with `box`, which must contain every vertex
// (skinBounds does); they tile the box. Empty unless the complex is 3D. Logs
// volume and prism orthogonality checks.
MixedComplex buildMixedComplex(const PowerComplex& complex, const Aabb& box, double s);

} // namespace skin::geometry
