#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/ConvexPolytope.hpp"

namespace skin::geometry {

inline constexpr uint8_t kNoDimension = UINT8_MAX;

enum class SkinMesher : uint8_t { MarchingTets, Cgal, KruithofVegter };

// An indexed triangle mesh of the skin. Triangles are counter-clockwise about
// the outward normal. Per-triangle provenance is kept for the mixed complex
// mesher; other meshers leave it untagged.
struct SkinMesh {
    std::vector<glm::dvec3> positions;
    std::vector<glm::dvec3> normals; // unit, outward
    std::vector<std::array<uint32_t, 3>> triangles;
    std::vector<uint32_t> triangleCell;     // index into the mesher's cells, or kUntagged
    std::vector<uint8_t> triangleDimension; // of the cell's Delaunay simplex, or kNoDimension

    bool empty() const { return triangles.empty(); }
};

// Combinatorial checks on a mesh, per connected component.
struct SkinMeshStats {
    size_t vertices = 0; // referenced by a triangle
    size_t edges = 0;
    size_t triangles = 0;
    size_t nonManifoldEdges = 0; // not in exactly 2 triangles
    std::vector<int64_t> eulerCharacteristics; // V - E + F per component
};

SkinMeshStats computeSkinMeshStats(const SkinMesh& mesh);

} // namespace skin::geometry
