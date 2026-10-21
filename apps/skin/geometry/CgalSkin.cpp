#include "skin/geometry/CgalSkin.hpp"

// CGAL's skin surface code trips MSVC backend warnings (unreachable code,
// maybe-uninitialised), reported at instantiation, so a push/pop around the
// includes doesn't reach them.
#ifdef _MSC_VER
#pragma warning(disable : 4701 4702)
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_triangle_primitive_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Skin_surface_3.h>
#include <CGAL/Skin_surface_traits_3.h>
#include <CGAL/mesh_skin_surface_3.h>
#include <CGAL/subdivide_skin_surface_mesh_3.h>

#include "skin/geometry/MixedComplex.hpp"
#include "skin/geometry/SkinQuadric.hpp"
#include "skin/geometry/WeightedPoint.hpp"

namespace skin::geometry {

namespace {

using K = CGAL::Exact_predicates_inexact_constructions_kernel;
using SkinSurface = CGAL::Skin_surface_3<CGAL::Skin_surface_traits_3<K>>;
using Polyhedron = CGAL::Polyhedron_3<K>;
using Triangles = std::vector<K::Triangle_3>;
using Tree =
    CGAL::AABB_tree<CGAL::AABB_traits_3<K, CGAL::AABB_triangle_primitive_3<K, Triangles::const_iterator>>>;

constexpr double LOCATION_TOLERANCE = 1e-9;

glm::dvec3 toGlm(const K::Point_3& p) {
    return {CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())};
}

K::Point_3 toCgal(const glm::dvec3& p) {
    return {p.x, p.y, p.z};
}

void fillAreaWeightedNormals(SkinMesh& mesh) {
    mesh.normals.assign(mesh.positions.size(), glm::dvec3(0.0));
    for (const auto& [a, b, c] : mesh.triangles) {
        const glm::dvec3 n =
            glm::cross(mesh.positions[b] - mesh.positions[a], mesh.positions[c] - mesh.positions[a]);
        mesh.normals[a] += n;
        mesh.normals[b] += n;
        mesh.normals[c] += n;
    }
    for (glm::dvec3& n : mesh.normals) {
        const double length = glm::length(n);
        if (length > 0.0) {
            n /= length;
        }
    }
}

// Mixed cells as half-space intersections, for point location.
struct CellPlanes {
    std::vector<glm::dvec4> planes; // outward unit normal, offset
    glm::dvec3 lo;
    glm::dvec3 hi;
};

std::vector<CellPlanes> cellPlanes(const MixedComplexTopology& topology) {
    const auto& points = topology.points;
    std::vector<CellPlanes> cells;
    cells.reserve(topology.cellFaces.size());
    for (const auto& faces : topology.cellFaces) {
        CellPlanes cell{.planes = {},
                        .lo = glm::dvec3(std::numeric_limits<double>::infinity()),
                        .hi = glm::dvec3(-std::numeric_limits<double>::infinity())};
        glm::dvec3 centroid(0.0);
        size_t count = 0;
        for (const auto& ring : faces) {
            for (uint32_t id : ring) {
                centroid += points[id];
                cell.lo = glm::min(cell.lo, points[id]);
                cell.hi = glm::max(cell.hi, points[id]);
                ++count;
            }
        }
        centroid /= static_cast<double>(std::max<size_t>(count, 1));
        for (const auto& ring : faces) {
            glm::dvec3 normal(0.0);
            for (size_t k = 0; k < ring.size(); ++k) {
                normal += glm::cross(points[ring[k]], points[ring[(k + 1) % ring.size()]]);
            }
            const double length = glm::length(normal);
            if (length == 0.0) {
                continue;
            }
            normal /= length;
            double offset = glm::dot(normal, points[ring[0]]);
            if (glm::dot(normal, centroid) > offset) {
                normal = -normal;
                offset = -offset;
            }
            cell.planes.emplace_back(normal, offset);
        }
        cells.push_back(std::move(cell));
    }
    return cells;
}

// Max and mean distance from each point to the triangles.
std::pair<double, double> distances(const std::vector<glm::dvec3>& from, const SkinMesh& to) {
    if (from.empty() || to.empty()) {
        return {0.0, 0.0};
    }
    Triangles triangles;
    triangles.reserve(to.triangles.size());
    for (const auto& [a, b, c] : to.triangles) {
        triangles.emplace_back(toCgal(to.positions[a]), toCgal(to.positions[b]),
                               toCgal(to.positions[c]));
    }
    Tree tree(triangles.begin(), triangles.end());
    tree.accelerate_distance_queries();
    double worst = 0.0;
    double sum = 0.0;
    for (const glm::dvec3& p : from) {
        const double d = std::sqrt(CGAL::to_double(tree.squared_distance(toCgal(p))));
        worst = std::max(worst, d);
        sum += d;
    }
    return {worst, sum / static_cast<double>(from.size())};
}

} // namespace

SkinMesh meshSkinWithCgal(std::span<const WeightedPoint> points, double s, int subdivisions) {
    SkinMesh mesh;
    if (points.empty()) {
        return mesh;
    }
    std::vector<K::Weighted_point_3> balls;
    balls.reserve(points.size());
    for (const WeightedPoint& point : points) {
        balls.emplace_back(K::Point_3(point.position.x, point.position.y, point.position.z),
                           point.weight);
    }
    const SkinSurface skin(balls.begin(), balls.end(), s, /*grow_balls=*/false);
    Polyhedron polyhedron;
    CGAL::mesh_skin_surface_3(skin, polyhedron);
    if (subdivisions > 0) {
        CGAL::subdivide_skin_surface_mesh_3(skin, polyhedron, subdivisions);
    }

    std::unordered_map<const void*, uint32_t> index;
    for (auto v = polyhedron.vertices_begin(); v != polyhedron.vertices_end(); ++v) {
        index.emplace(&*v, static_cast<uint32_t>(mesh.positions.size()));
        mesh.positions.push_back(toGlm(v->point()));
    }
    for (auto f = polyhedron.facets_begin(); f != polyhedron.facets_end(); ++f) {
        std::vector<uint32_t> ring;
        auto h = f->facet_begin();
        do {
            ring.push_back(index.at(&*h->vertex()));
        } while (++h != f->facet_begin());
        for (size_t k = 1; k + 1 < ring.size(); ++k) {
            mesh.triangles.push_back({ring[0], ring[k], ring[k + 1]});
        }
    }

    // Make the winding outward: positive enclosed volume.
    double volume = 0.0;
    for (const auto& [a, b, c] : mesh.triangles) {
        volume += glm::dot(mesh.positions[a], glm::cross(mesh.positions[b], mesh.positions[c]));
    }
    if (volume < 0.0) {
        for (auto& triangle : mesh.triangles) {
            std::swap(triangle[1], triangle[2]);
        }
    }
    mesh.triangleCell.assign(mesh.triangles.size(), kUntagged);
    mesh.triangleDimension.assign(mesh.triangles.size(), kNoDimension);
    fillAreaWeightedNormals(mesh);
    return mesh;
}

SkinComparison compareSkins(const SkinMesh& ours, const SkinMesh& cgal,
                            const MixedComplexTopology& topology,
                            const std::vector<SkinQuadric>& quadrics) {
    SkinComparison result;
    result.oursVertices = ours.positions.size();
    result.oursTriangles = ours.triangles.size();
    result.cgalVertices = cgal.positions.size();
    result.cgalTriangles = cgal.triangles.size();

    const std::vector<CellPlanes> cells = cellPlanes(topology);
    double extent = 0.0;
    for (const glm::dvec3& p : topology.points) {
        const glm::dvec3 a = glm::abs(p);
        extent = std::max({extent, a.x, a.y, a.z});
    }
    const double eps = LOCATION_TOLERANCE * (1.0 + extent);

    double residualSum = 0.0;
    for (const glm::dvec3& y : cgal.positions) {
        for (size_t c = 0; c < cells.size(); ++c) {
            const CellPlanes& cell = cells[c];
            if (glm::any(glm::lessThan(y, cell.lo - eps)) ||
                glm::any(glm::greaterThan(y, cell.hi + eps))) {
                continue;
            }
            const bool inside = std::all_of(cell.planes.begin(), cell.planes.end(),
                                            [&](const glm::dvec4& plane) {
                                                return glm::dot(glm::dvec3(plane), y) - plane.w <=
                                                       eps;
                                            });
            if (!inside) {
                continue;
            }
            const double gradient = glm::length(quadrics[c].gradient(y));
            const double residual =
                gradient > 0.0 ? std::abs(quadrics[c].value(y)) / gradient : 0.0;
            result.maxResidual = std::max(result.maxResidual, residual);
            residualSum += residual;
            ++result.located;
            break;
        }
    }
    if (result.located > 0) {
        result.meanResidual = residualSum / static_cast<double>(result.located);
    }

    std::tie(result.maxCgalToOurs, result.meanCgalToOurs) = distances(cgal.positions, ours);
    std::tie(result.maxOursToCgal, result.meanOursToCgal) = distances(ours.positions, cgal);
    return result;
}

} // namespace skin::geometry
