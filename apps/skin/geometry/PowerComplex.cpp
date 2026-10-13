#include "skin/geometry/PowerComplex.hpp"

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Regular_triangulation_3.h>
#include <CGAL/Triangulation_cell_base_with_info_3.h>
#include <CGAL/Triangulation_vertex_base_with_info_3.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <utility>

#include <nacre/core/Logger.hpp>

namespace skin::geometry {

namespace {

using K = CGAL::Exact_predicates_exact_constructions_kernel;

// Per-cell bookkeeping so "which tet / which triangle is this" is an O(1)
// lookup instead of a hash on CGAL handles.
struct CellInfo {
    uint32_t tet = kInfinite;
    std::array<uint32_t, 4> facet = {kInfinite, kInfinite, kInfinite, kInfinite};
};

// Vertex info starts as the input index (so hidden points can be detected),
// then is overwritten with our own vertex index once the triangulation is
// built.
using Vb = CGAL::Triangulation_vertex_base_with_info_3<
    uint32_t, K, CGAL::Regular_triangulation_vertex_base_3<K>>;
using Cb = CGAL::Triangulation_cell_base_with_info_3<
    CellInfo, K, CGAL::Regular_triangulation_cell_base_3<K>>;
using Tds = CGAL::Triangulation_data_structure_3<Vb, Cb>;
using RT = CGAL::Regular_triangulation_3<K, Tds>;

glm::dvec3 toDvec3(const RT::Bare_point& p) {
    return {CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())};
}

uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | b;
}

// Worst error before a sanity check is reported as a warning.
constexpr double kSanityTolerance = 1e-6;

} // namespace

struct PowerComplex::Impl {
    nacre::core::Logger logger{"PowerComplex"};
    RT rt;

    std::vector<DelaunayVertex> vertices;
    std::vector<DelaunayEdge> edges;
    std::vector<DelaunayTriangle> triangles;
    std::vector<DelaunayTet> tets;

    std::vector<glm::dvec3> voronoiVertices;
    std::vector<VoronoiEdge> voronoiEdges;
    std::vector<VoronoiFace> voronoiFaces;
    std::vector<VoronoiCell> voronoiCells;

    std::vector<uint32_t> hiddenInputs;

    void clear() {
        rt.clear();
        vertices.clear();
        edges.clear();
        triangles.clear();
        tets.clear();
        voronoiVertices.clear();
        voronoiEdges.clear();
        voronoiFaces.clear();
        voronoiCells.clear();
        hiddenInputs.clear();
    }

    void logSanityChecks() const;
};

PowerComplex::PowerComplex() : impl(std::make_unique<Impl>()) {}
PowerComplex::~PowerComplex() = default;

void PowerComplex::build(std::span<const WeightedPoint> points) {
    Impl& m = *impl;
    m.clear();

    // Range insert with info: spatially sorted, and hidden points simply never
    // get a vertex, so their input index never shows up below.
    std::vector<std::pair<RT::Weighted_point, uint32_t>> input;
    input.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const WeightedPoint& p = points[i];
        RT::Bare_point position(p.position.x, p.position.y, p.position.z);
        input.emplace_back(RT::Weighted_point(position, p.weight), static_cast<uint32_t>(i));
    }
    m.rt.insert(input.begin(), input.end());

    // ---- Vertices ----
    std::vector<bool> inputUsed(points.size(), false);
    for (auto v = m.rt.finite_vertices_begin(); v != m.rt.finite_vertices_end(); ++v) {
        const uint32_t inputIndex = v->info();
        const uint32_t index = static_cast<uint32_t>(m.vertices.size());
        v->info() = index;
        inputUsed[inputIndex] = true;

        const glm::dvec3 position = toDvec3(v->point().point());
        const double weight = CGAL::to_double(v->point().weight());
        m.vertices.push_back({position, weight, inputIndex});
    }
    for (size_t i = 0; i < inputUsed.size(); ++i) {
        if (!inputUsed[i]) {
            m.hiddenInputs.push_back(static_cast<uint32_t>(i));
        }
    }

    // ---- Edges ----
    // Handles are kept so the Voronoi faces below are built in exactly the same
    // order, keeping face i aligned with edge i.
    std::vector<RT::Edge> edgeHandles;
    std::unordered_map<uint64_t, uint32_t> edgeIndex;
    for (auto e = m.rt.finite_edges_begin(); e != m.rt.finite_edges_end(); ++e) {
        const uint32_t a = e->first->vertex(e->second)->info();
        const uint32_t b = e->first->vertex(e->third)->info();
        edgeIndex.emplace(edgeKey(a, b), static_cast<uint32_t>(m.edges.size()));
        m.edges.push_back({{a, b}});
        edgeHandles.push_back(*e);
    }

    const bool full = m.rt.dimension() == 3;

    // ---- Tets + Voronoi vertices ----
    // Done before the triangles so each triangle can look up the tets on
    // either side of it.
    if (full) {
        for (auto c = m.rt.finite_cells_begin(); c != m.rt.finite_cells_end(); ++c) {
            RT::Cell_handle cell = c;
            cell->info().tet = static_cast<uint32_t>(m.tets.size());
            m.tets.push_back({{cell->vertex(0)->info(), cell->vertex(1)->info(),
                               cell->vertex(2)->info(), cell->vertex(3)->info()}});

            // The weighted circumcentre (orthocentre) of the tet.
            m.voronoiVertices.push_back(toDvec3(m.rt.dual(cell)));
        }
    }

    // ---- Triangles + Voronoi edges ----
    for (auto f = m.rt.finite_facets_begin(); f != m.rt.finite_facets_end(); ++f) {
        const RT::Cell_handle cell = f->first;
        const int i = f->second;
        const uint32_t triangle = static_cast<uint32_t>(m.triangles.size());
        m.triangles.push_back({{cell->vertex((i + 1) & 3)->info(),
                                cell->vertex((i + 2) & 3)->info(),
                                cell->vertex((i + 3) & 3)->info()}});
        if (!full) {
            continue;
        }

        const RT::Cell_handle neighbor = cell->neighbor(i);
        const int neighborIndex = neighbor->index(cell);
        cell->info().facet[i] = triangle;
        neighbor->info().facet[neighborIndex] = triangle;

        // Normalise so the finite tet comes first.
        RT::Cell_handle inner = cell;
        RT::Cell_handle outer = neighbor;
        int innerIndex = i;
        if (m.rt.is_infinite(inner)) {
            std::swap(inner, outer);
            innerIndex = neighborIndex;
        }

        VoronoiEdge edge{};
        edge.tets = {inner->info().tet, m.rt.is_infinite(outer) ? kInfinite : outer->info().tet};
        edge.rayDir = glm::dvec3(0.0);
        if (edge.tets[1] == kInfinite) {
            // Hull triangle: the dual is a ray along the outward normal, i.e.
            // pointing away from the finite tet's opposite vertex.
            const auto& verts = m.vertices;
            const glm::dvec3 p0 = verts[inner->vertex((innerIndex + 1) & 3)->info()].position;
            const glm::dvec3 p1 = verts[inner->vertex((innerIndex + 2) & 3)->info()].position;
            const glm::dvec3 p2 = verts[inner->vertex((innerIndex + 3) & 3)->info()].position;
            const glm::dvec3 apex = verts[inner->vertex(innerIndex)->info()].position;
            glm::dvec3 normal = glm::cross(p1 - p0, p2 - p0);
            if (glm::dot(normal, apex - p0) > 0.0) {
                normal = -normal;
            }
            edge.rayDir = glm::normalize(normal);
        }
        m.voronoiEdges.push_back(edge);
    }

    if (full) {
        // ---- Voronoi faces (dual of each edge) ----
        std::vector<RT::Cell_handle> around;
        for (const RT::Edge& e : edgeHandles) {
            around.clear();
            RT::Cell_circulator circulator = m.rt.incident_cells(e);
            const RT::Cell_circulator done = circulator;
            do {
                around.push_back(circulator);
            } while (++circulator != done);

            VoronoiFace face{};
            face.bounded = std::none_of(around.begin(), around.end(), [&m](RT::Cell_handle c) {
                return m.rt.is_infinite(c);
            });

            // For an unbounded face, start at the first finite cell after an
            // infinite one so the finite cells come out as one ordered run.
            const size_t count = around.size();
            size_t start = 0;
            if (!face.bounded) {
                for (size_t k = 0; k < count; ++k) {
                    if (m.rt.is_infinite(around[k]) && !m.rt.is_infinite(around[(k + 1) % count])) {
                        start = (k + 1) % count;
                        break;
                    }
                }
            }

            face.ring.reserve(count);
            for (size_t k = 0; k < count; ++k) {
                const RT::Cell_handle c = around[(start + k) % count];
                if (!m.rt.is_infinite(c)) {
                    face.ring.push_back(c->info().tet);
                }
            }
            m.voronoiFaces.push_back(std::move(face));
        }

        // ---- Voronoi cells (dual of each vertex) ----
        m.voronoiCells.resize(m.vertices.size());
        std::vector<RT::Edge> incident;
        for (auto v = m.rt.finite_vertices_begin(); v != m.rt.finite_vertices_end(); ++v) {
            const RT::Vertex_handle vertex = v;
            incident.clear();
            m.rt.finite_incident_edges(vertex, std::back_inserter(incident));

            VoronoiCell& cell = m.voronoiCells[vertex->info()];
            cell.bounded = true;
            cell.faces.reserve(incident.size());
            for (const RT::Edge& e : incident) {
                const uint32_t a = e.first->vertex(e.second)->info();
                const uint32_t b = e.first->vertex(e.third)->info();
                const uint32_t face = edgeIndex.at(edgeKey(a, b));
                cell.faces.push_back(face);
                cell.bounded = cell.bounded && m.voronoiFaces[face].bounded;
            }
        }
    }

    m.logSanityChecks();
}

void PowerComplex::Impl::logSanityChecks() const {
    logger.print("Dimension {}: {} vertices, {} edges, {} triangles, {} tets, {} hidden inputs",
                 rt.dimension(), vertices.size(), edges.size(), triangles.size(), tets.size(),
                 hiddenInputs.size());

    if (vertices.empty()) {
        return;
    }

    // A triangulated convex hull (in any dimension) is a ball: V - E + F - T = 1.
    const long long euler = static_cast<long long>(vertices.size()) -
                            static_cast<long long>(edges.size()) +
                            static_cast<long long>(triangles.size()) -
                            static_cast<long long>(tets.size());
    if (euler != 1) {
        logger.warn("Euler characteristic is {}, expected 1", euler);
    }

    if (tets.empty()) {
        return;
    }

    // Each Voronoi vertex must have equal power distance |x - p|^2 - w to all
    // four weighted points of its tet.
    double worstPower = 0.0;
    for (size_t t = 0; t < tets.size(); ++t) {
        const glm::dvec3& x = voronoiVertices[t];
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        for (uint32_t v : tets[t].v) {
            const glm::dvec3 d = x - vertices[v].position;
            const double power = glm::dot(d, d) - vertices[v].weight;
            lo = std::min(lo, power);
            hi = std::max(hi, power);
        }
        const double scale = std::max({1.0, std::abs(lo), std::abs(hi)});
        worstPower = std::max(worstPower, (hi - lo) / scale);
    }

    // Each bounded Voronoi face must be perpendicular to its dual Delaunay
    // edge -- the defining property of a power diagram.
    double worstOrthogonality = 0.0;
    for (size_t i = 0; i < edges.size(); ++i) {
        const VoronoiFace& face = voronoiFaces[i];
        if (!face.bounded || face.ring.size() < 3) {
            continue;
        }
        const glm::dvec3 axis =
            glm::normalize(vertices[edges[i].v[1]].position - vertices[edges[i].v[0]].position);
        const glm::dvec3& origin = voronoiVertices[face.ring[0]];
        for (size_t k = 1; k < face.ring.size(); ++k) {
            const glm::dvec3 d = voronoiVertices[face.ring[k]] - origin;
            const double length = glm::length(d);
            if (length < 1e-12) {
                continue; // coincident Voronoi vertices (co-spherical points)
            }
            worstOrthogonality = std::max(worstOrthogonality, std::abs(glm::dot(d / length, axis)));
        }
    }

    logger.print("Worst power-distance error {:.3e}, worst face/edge orthogonality error {:.3e}",
                 worstPower, worstOrthogonality);
    if (worstPower > kSanityTolerance || worstOrthogonality > kSanityTolerance) {
        logger.warn("Power complex sanity check exceeded tolerance {:.0e}", kSanityTolerance);
    }
}

int PowerComplex::dimension() const { return impl->rt.dimension(); }

const std::vector<DelaunayVertex>& PowerComplex::vertices() const { return impl->vertices; }
const std::vector<DelaunayEdge>& PowerComplex::edges() const { return impl->edges; }
const std::vector<DelaunayTriangle>& PowerComplex::triangles() const { return impl->triangles; }
const std::vector<DelaunayTet>& PowerComplex::tets() const { return impl->tets; }

const std::vector<glm::dvec3>& PowerComplex::voronoiVertices() const {
    return impl->voronoiVertices;
}
const std::vector<VoronoiEdge>& PowerComplex::voronoiEdges() const { return impl->voronoiEdges; }
const std::vector<VoronoiFace>& PowerComplex::voronoiFaces() const { return impl->voronoiFaces; }
const std::vector<VoronoiCell>& PowerComplex::voronoiCells() const { return impl->voronoiCells; }

const std::vector<uint32_t>& PowerComplex::hiddenInputs() const { return impl->hiddenInputs; }

} // namespace skin::geometry
