#include "skin/geometry/SkinQuadric.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "skin/geometry/ConvexPolytope.hpp"
#include "skin/geometry/MixedComplex.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include <nacre/core/Logger.hpp>

namespace skin::geometry {

namespace {

constexpr double BOX_TOLERANCE = 1e-9;

// Every point of the ring on one face plane of the box.
bool onBox(const std::vector<uint32_t>& ring, const std::vector<glm::dvec3>& points,
           const Aabb& box) {
    const double eps = BOX_TOLERANCE * (1.0 + glm::length(box.max - box.min));
    for (int axis = 0; axis < 3; ++axis) {
        for (const double plane : {box.min[axis], box.max[axis]}) {
            const bool all = std::all_of(ring.begin(), ring.end(), [&](uint32_t id) {
                return std::abs(points[id][axis] - plane) <= eps;
            });
            if (all) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

SkinQuadric makeSkinQuadric(const PowerComplex& complex, uint32_t dimension, uint32_t simplex,
                            double s) {
    const auto& vertices = complex.vertices();
    const glm::dmat3 identity(1.0);

    glm::dvec3 centre(0.0);
    glm::dmat3 projector(0.0);
    uint32_t anyVertex = 0;
    switch (dimension) {
    case 0:
        anyVertex = simplex;
        centre = vertices[anyVertex].position;
        break;
    case 1: {
        const auto& edge = complex.edges()[simplex];
        const DelaunayVertex& a = vertices[edge.v[0]];
        const DelaunayVertex& b = vertices[edge.v[1]];
        const glm::dvec3 ab = b.position - a.position;
        const double length2 = glm::dot(ab, ab);
        const double t = (length2 + a.weight - b.weight) / (2.0 * length2);
        anyVertex = edge.v[0];
        centre = a.position + t * ab;
        const glm::dvec3 e = ab / std::sqrt(length2);
        projector = glm::outerProduct(e, e);
        break;
    }
    case 2: {
        // The Voronoi edge is perpendicular to the triangle and passes
        // through z_X; tets[0] is always finite.
        const auto& triangle = complex.triangles()[simplex];
        const glm::dvec3& a = vertices[triangle.v[0]].position;
        const glm::dvec3& b = vertices[triangle.v[1]].position;
        const glm::dvec3& c = vertices[triangle.v[2]].position;
        const glm::dvec3 n = glm::normalize(glm::cross(b - a, c - a));
        const glm::dvec3& y = complex.voronoiVertices()[complex.voronoiEdges()[simplex].tets[0]];
        anyVertex = triangle.v[0];
        centre = y - glm::dot(y - a, n) * n;
        projector = identity - glm::outerProduct(n, n);
        break;
    }
    default:
        anyVertex = complex.tets()[simplex].v[0];
        centre = complex.voronoiVertices()[simplex];
        projector = identity;
        break;
    }

    const DelaunayVertex& v = vertices[anyVertex];
    const glm::dvec3 offset = v.position - centre;
    const double weight = glm::dot(offset, offset) - v.weight;

    double formNorm = 0.0;
    if (dimension < 3) {
        formNorm = std::max(formNorm, 1.0 / s);
    }
    if (dimension > 0) {
        formNorm = std::max(formNorm, 1.0 / (1.0 - s));
    }
    const double scale = std::sqrt((dimension == 3 ? 1.0 - s : s) * std::abs(weight));

    return {.centre = centre,
            .weight = weight,
            .form = (identity - projector) / s - projector / (1.0 - s),
            .projector = projector,
            .formNorm = formNorm,
            .scale = scale};
}

std::vector<SkinQuadric> buildSkinQuadrics(const PowerComplex& complex, const MixedComplex& mixed,
                                           double s) {
    std::vector<SkinQuadric> quadrics;
    quadrics.reserve(mixed.cells.size());
    for (const MixedCell& cell : mixed.cells) {
        quadrics.push_back(makeSkinQuadric(complex, cell.dimension, cell.simplex, s));
    }
    return quadrics;
}

void logWallAgreement(const MixedComplex& mixed, const MixedComplexTopology& topology,
                      const std::vector<SkinQuadric>& quadrics, const Aabb& box) {
    std::map<std::vector<uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>> walls;
    for (uint32_t c = 0; c < topology.cellFaces.size(); ++c) {
        for (uint32_t f = 0; f < topology.cellFaces[c].size(); ++f) {
            std::vector<uint32_t> key = topology.cellFaces[c][f];
            std::sort(key.begin(), key.end());
            walls[std::move(key)].emplace_back(c, f);
        }
    }

    const auto& points = topology.points;
    uint32_t shared = 0;
    uint32_t unmatched = 0;
    uint32_t overmatched = 0;
    double worstValue = 0.0;
    double worstGradient = 0.0;
    for (const auto& [key, owners] : walls) {
        if (owners.size() == 1) {
            if (!onBox(key, points, box)) {
                ++unmatched;
            }
            continue;
        }
        if (owners.size() > 2) {
            ++overmatched;
            continue;
        }
        ++shared;

        // Corners, edge midpoints and the centroid.
        std::vector<glm::dvec3> samples;
        const auto& ring = topology.cellFaces[owners[0].first][owners[0].second];
        glm::dvec3 centroid(0.0);
        for (size_t k = 0; k < ring.size(); ++k) {
            const glm::dvec3& p = points[ring[k]];
            samples.push_back(p);
            samples.push_back(0.5 * (p + points[ring[(k + 1) % ring.size()]]));
            centroid += p;
        }
        samples.push_back(centroid / static_cast<double>(ring.size()));

        const SkinQuadric& q0 = quadrics[owners[0].first];
        const SkinQuadric& q1 = quadrics[owners[1].first];
        for (const glm::dvec3& x : samples) {
            worstValue = std::max(worstValue, std::abs(q0.value(x) - q1.value(x)));
            worstGradient =
                std::max(worstGradient, glm::length(q0.gradient(x) - q1.gradient(x)));
        }
    }

    const nacre::core::Logger logger{"SkinQuadric"};
    logger.print("{} cells, {} welded points; {} shared walls, worst |dF| {:.3e}, worst "
                 "|d grad F| {:.3e}; {} unmatched interior walls, {} walls in > 2 cells",
                 mixed.cells.size(), points.size(), shared, worstValue, worstGradient, unmatched,
                 overmatched);
    if (unmatched > 0 || overmatched > 0) {
        logger.warn("Mixed cells do not meet face to face");
    }
}

} // namespace skin::geometry
