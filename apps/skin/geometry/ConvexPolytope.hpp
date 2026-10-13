#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace skin::geometry {

inline constexpr uint32_t kUntagged = UINT32_MAX;

struct Aabb {
    glm::dvec3 min{0.0};
    glm::dvec3 max{0.0};

    glm::dvec3 centre() const { return 0.5 * (min + max); }
    double volume() const {
        const glm::dvec3 extent = max - min;
        return extent.x * extent.y * extent.z;
    }
};

// A bounded convex polytope as its boundary polygons. Each face keeps the tag
// of the plane that produced it (kUntagged for box and input faces).
struct ConvexPolytope {
    struct Face {
        std::vector<glm::dvec3> ring; // cyclic order, either orientation
        uint32_t tag = kUntagged;
    };

    std::vector<Face> faces;

    static ConvexPolytope fromBox(const Aabb& box);
    // The polygons must bound a convex region.
    static ConvexPolytope fromPolygons(std::vector<std::vector<glm::dvec3>> polygons);

    // Keeps {x : dot(normal, x) <= offset}; the cut face gets `tag`. Returns
    // true if anything was removed.
    bool clip(const glm::dvec3& normal, double offset, uint32_t tag = kUntagged);
    bool clip(const Aabb& box);

    bool empty() const { return faces.size() < 4; }
    const Face* findFace(uint32_t tag) const;
    glm::dvec3 vertexMean() const; // an interior point
    double volume() const;
};

} // namespace skin::geometry
