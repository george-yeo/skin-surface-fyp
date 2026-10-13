#pragma once

#include <array>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/ConvexPolytope.hpp"
#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

// The power diagram intersected with a box, indexed like PowerComplex:
//   cells[i] <-> vertices()[i], faces[i] <-> edges()[i], edges[i] <-> triangles()[i]
// Every cell is closed and the cells tile the box. A cell's face tagged e is
// faces[e]; untagged faces lie on the box.
struct BoundedVoronoi {
    Aabb box;
    std::vector<ConvexPolytope> cells;
    std::vector<std::vector<glm::dvec3>> faces;                  // empty if outside the box
    std::vector<std::optional<std::array<glm::dvec3, 2>>> edges; // nullopt if outside
};

// Bounds of every ball, padded by `padding` times the diagonal. The skin lies
// inside the union of the balls, so it lies inside this box.
Aabb skinBounds(const PowerComplex& complex, double padding = 0.05);

// For drawing the diagram only: the skin box grown to include every Voronoi
// vertex within `reach` skin-box diagonals of its centre (farther ones come
// from near-flat hull tets), padded by `padding` times the skin box diagonal
// so rays show as stubs.
Aabb displayBounds(const PowerComplex& complex, const Aabb& skinBox, double padding = 0.25,
                   double reach = 3.0);

Aabb scaledAboutCentre(const Aabb& box, double factor);

// Only the box is set unless the complex is 3D. Logs a volume tiling check.
BoundedVoronoi buildBoundedVoronoi(const PowerComplex& complex, const Aabb& box);

} // namespace skin::geometry
