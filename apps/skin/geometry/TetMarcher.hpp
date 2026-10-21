#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/SkinMesh.hpp"
#include "skin/geometry/SkinQuadric.hpp"

namespace skin::geometry {

// Marching tetrahedra over tets that each lie in one mixed cell. F is
// evaluated once per point, with the quadric of the first tet that reaches it
// (F is C^1 across walls), and each edge's crossing is solved once, so tets
// sharing a face share its mesh vertices and the mesh is watertight. The
// points and their cached values belong to the caller, which may keep adding
// points (values NaN until evaluated).
class TetMarcher {
    public:
    TetMarcher(const std::vector<glm::dvec3>& points, std::vector<double>& values,
               const std::vector<SkinQuadric>& quadrics, std::vector<uint8_t> cellDimensions);

    double value(uint32_t point, uint32_t cell);
    void march(const std::array<uint32_t, 4>& tet, uint32_t cell);
    // Gives zero-gradient vertices (cone points) the area-weighted normal and
    // hands over the mesh.
    SkinMesh finish();

    private:
    const std::vector<glm::dvec3>& points;
    std::vector<double>& values;
    const std::vector<SkinQuadric>& quadrics;
    std::vector<uint8_t> cellDimensions;
    std::unordered_map<uint64_t, uint32_t> crossings; // edge -> mesh vertex
    SkinMesh mesh;

    uint32_t crossing(uint32_t a, uint32_t b, uint32_t cell);
    void addTriangle(uint32_t a, uint32_t b, uint32_t c, const glm::dvec3& outward,
                     uint32_t cell);
};

} // namespace skin::geometry
