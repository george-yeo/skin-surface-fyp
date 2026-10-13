#include "skin/geometry/BoundedVoronoi.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "skin/geometry/PowerComplex.hpp"
#include <nacre/core/Logger.hpp>

namespace skin::geometry {

namespace {

constexpr double TILING_TOLERANCE = 1e-9;
constexpr double MIN_PADDING = 1e-3;
constexpr double INF = std::numeric_limits<double>::infinity();

// Liang-Barsky: the part of p + t * dir, t in [t0, t1], inside the box.
std::optional<std::array<glm::dvec3, 2>> clipSegment(const glm::dvec3& p, const glm::dvec3& dir,
                                                     double t0, double t1, const Aabb& box) {
    for (int axis = 0; axis < 3; ++axis) {
        if (dir[axis] == 0.0) {
            if (p[axis] < box.min[axis] || p[axis] > box.max[axis]) {
                return std::nullopt;
            }
            continue;
        }
        double a = (box.min[axis] - p[axis]) / dir[axis];
        double b = (box.max[axis] - p[axis]) / dir[axis];
        if (a > b) {
            std::swap(a, b);
        }
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) {
            return std::nullopt;
        }
    }
    if (t1 == INF) {
        return std::nullopt;
    }
    return std::array<glm::dvec3, 2>{p + t0 * dir, p + t1 * dir};
}

} // namespace

Aabb skinBounds(const PowerComplex& complex, double padding) {
    const auto& vertices = complex.vertices();
    if (vertices.empty()) {
        return {};
    }

    Aabb box{.min = glm::dvec3(INF), .max = glm::dvec3(-INF)};
    for (const DelaunayVertex& vertex : vertices) {
        const double radius = std::sqrt(std::max(vertex.weight, 0.0));
        box.min = glm::min(box.min, vertex.position - radius);
        box.max = glm::max(box.max, vertex.position + radius);
    }
    const double pad = std::max(padding * glm::length(box.max - box.min), MIN_PADDING);
    box.min -= pad;
    box.max += pad;
    return box;
}

Aabb displayBounds(const PowerComplex& complex, const Aabb& skinBox, double padding,
                   double reach) {
    const glm::dvec3 centre = skinBox.centre();
    const double diagonal = glm::length(skinBox.max - skinBox.min);
    const double maxDistance = reach * diagonal;
    Aabb box = skinBox;
    for (const glm::dvec3& vertex : complex.voronoiVertices()) {
        if (glm::length(vertex - centre) <= maxDistance) {
            box.min = glm::min(box.min, vertex);
            box.max = glm::max(box.max, vertex);
        }
    }
    box.min -= padding * diagonal;
    box.max += padding * diagonal;
    return box;
}

Aabb scaledAboutCentre(const Aabb& box, double factor) {
    const glm::dvec3 centre = box.centre();
    const glm::dvec3 half = 0.5 * factor * (box.max - box.min);
    return {.min = centre - half, .max = centre + half};
}

BoundedVoronoi buildBoundedVoronoi(const PowerComplex& complex, const Aabb& box) {
    BoundedVoronoi result{.box = box, .cells = {}, .faces = {}, .edges = {}};
    if (complex.tets().empty()) {
        return result;
    }

    const auto& vertices = complex.vertices();
    const auto& edges = complex.edges();
    std::vector<std::vector<uint32_t>> incident(vertices.size());
    for (uint32_t e = 0; e < edges.size(); ++e) {
        incident[edges[e].v[0]].push_back(e);
        incident[edges[e].v[1]].push_back(e);
    }

    // Each cell: the box cut by the power bisector with every Delaunay
    // neighbour, keeping the side where pi_i <= pi_j.
    const ConvexPolytope boxPolytope = ConvexPolytope::fromBox(box);
    uint32_t touchingBox = 0;
    result.cells.reserve(vertices.size());
    for (uint32_t i = 0; i < vertices.size(); ++i) {
        const glm::dvec3& zi = vertices[i].position;
        ConvexPolytope cell = boxPolytope;
        for (uint32_t e : incident[i]) {
            const uint32_t j = edges[e].v[0] == i ? edges[e].v[1] : edges[e].v[0];
            const glm::dvec3 axis = vertices[j].position - zi;
            const double t =
                0.5 + (vertices[i].weight - vertices[j].weight) / (2.0 * glm::dot(axis, axis));
            cell.clip(axis, glm::dot(axis, zi + t * axis), e);
            if (cell.empty()) {
                break;
            }
        }
        if (cell.findFace(kUntagged) != nullptr) {
            ++touchingBox;
        }
        result.cells.push_back(std::move(cell));
    }

    result.faces.resize(edges.size());
    for (uint32_t e = 0; e < edges.size(); ++e) {
        const uint32_t owner = std::min(edges[e].v[0], edges[e].v[1]);
        if (const ConvexPolytope::Face* face = result.cells[owner].findFace(e)) {
            result.faces[e] = face->ring;
        }
    }

    const auto& voronoiVertices = complex.voronoiVertices();
    result.edges.reserve(complex.voronoiEdges().size());
    for (const VoronoiEdge& edge : complex.voronoiEdges()) {
        const glm::dvec3& start = voronoiVertices[edge.tets[0]];
        if (edge.tets[1] == kInfinite) {
            result.edges.push_back(clipSegment(start, edge.rayDir, 0.0, INF, box));
        } else {
            result.edges.push_back(
                clipSegment(start, voronoiVertices[edge.tets[1]] - start, 0.0, 1.0, box));
        }
    }

    double total = 0.0;
    for (const ConvexPolytope& cell : result.cells) {
        total += cell.volume();
    }
    const double error = std::abs(total - box.volume()) / box.volume();

    const nacre::core::Logger logger{"BoundedVoronoi"};
    logger.print("{} cells, {} closed by the box; relative volume error {:.3e}",
                 result.cells.size(), touchingBox, error);
    if (error > TILING_TOLERANCE) {
        logger.warn("Clipped cells do not tile the box (tolerance {:.0e})", TILING_TOLERANCE);
    }
    return result;
}

} // namespace skin::geometry
