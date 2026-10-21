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

// The cells' corners merged into shared ids, so neighbouring cells index the
// same points on a shared wall. Face rings follow MixedCell::faces.
struct MixedComplexTopology {
    std::vector<glm::dvec3> points;
    std::vector<std::vector<std::vector<uint32_t>>> cellFaces; // [cell][face] -> cyclic ids
};

// Mixed cells intersected with `box`; they tile the box. Empty unless the
// complex is 3D. Logs volume and prism orthogonality checks.
MixedComplex buildMixedComplex(const PowerComplex& complex, const Aabb& box, double s);

// Merges corners closer than the clipping tolerance. Faces left with fewer
// than 3 distinct ids are dropped.
MixedComplexTopology weldMixedComplex(const MixedComplex& mixed);

} // namespace skin::geometry
