#include "skin/geometry/MixedComplex.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "skin/geometry/BoundedVoronoi.hpp"
#include "skin/geometry/ConvexPolytope.hpp"
#include "skin/geometry/PointWelder.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include <nacre/core/Logger.hpp>

namespace skin::geometry {

namespace {

constexpr double SANITY_TOLERANCE = 1e-6;
// Same relative tolerance ConvexPolytope clips with.
constexpr double WELD_EPSILON = 1e-10;
constexpr double TILING_TOLERANCE = 1e-9;
// Cells at or below this fraction of the box volume are treated as flat.
constexpr double MIN_RELATIVE_CELL_VOLUME = 1e-12;

using Polygons = std::vector<std::vector<glm::dvec3>>;

glm::dvec3 mixPoint(const glm::dvec3& simplexPoint, const glm::dvec3& dualPoint, double s) {
    return (1.0 - s) * simplexPoint + s * dualPoint;
}

// |cos| of the angle between two segments; 0 when either is degenerate.
double alignment(const glm::dvec3& u, const glm::dvec3& v) {
    const double lu = glm::length(u);
    const double lv = glm::length(v);
    if (lu < 1e-12 || lv < 1e-12) {
        return 0.0;
    }
    return std::abs(glm::dot(u / lu, v / lv));
}

// Faces of the prism between two copies of a polygon (cap0[k] <-> cap1[k]).
Polygons prismFaces(const std::vector<glm::dvec3>& cap0, const std::vector<glm::dvec3>& cap1) {
    Polygons faces{cap0, cap1};
    for (size_t k = 0; k < cap0.size(); ++k) {
        const size_t next = (k + 1) % cap0.size();
        faces.push_back({cap0[k], cap0[next], cap1[next], cap1[k]});
    }
    return faces;
}

} // namespace

MixedComplexTopology weldMixedComplex(const MixedComplex& mixed) {
    double extent = 0.0;
    for (const MixedCell& cell : mixed.cells) {
        for (const auto& face : cell.faces) {
            for (const glm::dvec3& p : face) {
                const glm::dvec3 a = glm::abs(p);
                extent = std::max({extent, a.x, a.y, a.z});
            }
        }
    }
    PointWelder welder(WELD_EPSILON * (1.0 + extent));

    MixedComplexTopology result;
    result.cellFaces.reserve(mixed.cells.size());
    for (const MixedCell& cell : mixed.cells) {
        auto& faces = result.cellFaces.emplace_back();
        for (const auto& face : cell.faces) {
            std::vector<uint32_t> ring;
            for (const glm::dvec3& p : face) {
                const uint32_t id = welder.idOf(p);
                if (ring.empty() || ring.back() != id) {
                    ring.push_back(id);
                }
            }
            while (ring.size() > 1 && ring.front() == ring.back()) {
                ring.pop_back();
            }
            if (ring.size() >= 3) {
                faces.push_back(std::move(ring));
            }
        }
    }
    result.points = welder.takePoints();
    return result;
}

MixedComplex buildMixedComplex(const PowerComplex& complex, const Aabb& box, double s) {
    MixedComplex result;
    if (complex.tets().empty()) {
        return result;
    }

    const auto& vertices = complex.vertices();

    // x = (1 - s) q + s y with x and q in a box H puts y in H scaled by
    // (2 - s) / s, so duals clipped to that lose nothing inside the box. H
    // must hold every vertex q, which ghost points put outside `box`.
    Aabb enclosing = box;
    for (const DelaunayVertex& vertex : vertices) {
        enclosing.min = glm::min(enclosing.min, vertex.position);
        enclosing.max = glm::max(enclosing.max, vertex.position);
    }
    const BoundedVoronoi duals =
        buildBoundedVoronoi(complex, scaledAboutCentre(enclosing, (2.0 - s) / s));

    const auto& edges = complex.edges();
    const auto& triangles = complex.triangles();
    const auto& tets = complex.tets();
    const auto& voronoiVertices = complex.voronoiVertices();

    double volume = 0.0;
    const auto emit = [&](uint32_t dimension, uint32_t simplex, Polygons polygons) {
        ConvexPolytope polytope = ConvexPolytope::fromPolygons(std::move(polygons));
        const bool cut = polytope.clip(box);
        if (polytope.empty()) {
            return;
        }
        // Exactly degenerate input (e.g. cospherical points) yields flat cells
        // that still have enough faces to pass empty(); they add no volume.
        const double cellVolume = polytope.volume();
        if (cellVolume <= MIN_RELATIVE_CELL_VOLUME * box.volume()) {
            return;
        }
        if (cut) {
            ++result.clipped[dimension];
        }
        volume += cellVolume;
        MixedCell cell{.dimension = dimension, .simplex = simplex, .faces = {}};
        for (ConvexPolytope::Face& face : polytope.faces) {
            cell.faces.push_back(std::move(face.ring));
        }
        result.cells.push_back(std::move(cell));
    };

    // Vertex p + Voronoi cell: the cell scaled by s about p.
    for (uint32_t i = 0; i < vertices.size(); ++i) {
        const ConvexPolytope& dual = duals.cells[i];
        if (dual.empty()) {
            continue;
        }
        const glm::dvec3& p = vertices[i].position;
        Polygons polygons;
        for (const ConvexPolytope::Face& face : dual.faces) {
            std::vector<glm::dvec3> polygon;
            for (const glm::dvec3& y : face.ring) {
                polygon.push_back(mixPoint(p, y, s));
            }
            polygons.push_back(std::move(polygon));
        }
        emit(0, i, std::move(polygons));
    }

    // Edge [a, b] + Voronoi face F (perpendicular to ab): a right prism.
    double worstEdgePrism = 0.0;
    for (uint32_t i = 0; i < edges.size(); ++i) {
        const std::vector<glm::dvec3>& dual = duals.faces[i];
        if (dual.size() < 3) {
            continue;
        }
        const glm::dvec3& a = vertices[edges[i].v[0]].position;
        const glm::dvec3& b = vertices[edges[i].v[1]].position;
        std::vector<glm::dvec3> capA;
        std::vector<glm::dvec3> capB;
        for (const glm::dvec3& y : dual) {
            capA.push_back(mixPoint(a, y, s));
            capB.push_back(mixPoint(b, y, s));
        }
        for (size_t k = 0; k + 1 < capA.size(); ++k) {
            worstEdgePrism = std::max(worstEdgePrism, alignment(capA[k + 1] - capA[k], b - a));
        }
        emit(1, i, prismFaces(capA, capB));
    }

    // Triangle + Voronoi edge [y0, y1] (perpendicular to the triangle): a
    // triangular prism.
    double worstTrianglePrism = 0.0;
    for (uint32_t i = 0; i < triangles.size(); ++i) {
        const auto& dual = duals.edges[i];
        if (!dual) {
            continue;
        }
        const glm::dvec3& y0 = (*dual)[0];
        const glm::dvec3& y1 = (*dual)[1];
        std::vector<glm::dvec3> cap0;
        std::vector<glm::dvec3> cap1;
        for (uint32_t v : triangles[i].v) {
            cap0.push_back(mixPoint(vertices[v].position, y0, s));
            cap1.push_back(mixPoint(vertices[v].position, y1, s));
        }
        for (size_t k = 0; k < 3; ++k) {
            worstTrianglePrism =
                std::max(worstTrianglePrism, alignment(cap0[(k + 1) % 3] - cap0[k], y1 - y0));
        }
        emit(2, i, prismFaces(cap0, cap1));
    }

    // Tet + its Voronoi vertex z: the tet scaled by (1 - s) about z.
    for (uint32_t i = 0; i < tets.size(); ++i) {
        const glm::dvec3& z = voronoiVertices[i];
        std::array<glm::dvec3, 4> p;
        for (size_t k = 0; k < 4; ++k) {
            p[k] = mixPoint(vertices[tets[i].v[k]].position, z, s);
        }
        Polygons polygons;
        for (size_t face = 0; face < 4; ++face) {
            polygons.push_back({p[(face + 1) % 4], p[(face + 2) % 4], p[(face + 3) % 4]});
        }
        emit(3, i, std::move(polygons));
    }

    const double error = std::abs(volume - box.volume()) / box.volume();
    const nacre::core::Logger logger{"MixedComplex"};
    logger.print("s = {}: {} cells, clipped {} V / {} E / {} F / {} T; relative volume error "
                 "{:.3e}; worst prism misalignment {:.3e} (edge), {:.3e} (triangle)",
                 s, result.cells.size(), result.clipped[0], result.clipped[1], result.clipped[2],
                 result.clipped[3], error, worstEdgePrism, worstTrianglePrism);
    if (error > TILING_TOLERANCE) {
        logger.warn("Mixed cells do not tile the box (tolerance {:.0e})", TILING_TOLERANCE);
    }
    if (worstEdgePrism > SANITY_TOLERANCE || worstTrianglePrism > SANITY_TOLERANCE) {
        logger.warn("Mixed cell prism check exceeded tolerance {:.0e}", SANITY_TOLERANCE);
    }
    return result;
}

} // namespace skin::geometry
