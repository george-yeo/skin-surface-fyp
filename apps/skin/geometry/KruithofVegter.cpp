#include "skin/geometry/KruithofVegter.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <unordered_map>
#include <utility>

#include "skin/geometry/PointWelder.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include "skin/geometry/SkinBuild.hpp"
#include "skin/geometry/TetMarcher.hpp"
#include <nacre/core/Logger.hpp>

namespace skin::geometry {

namespace {

// Slack on barycentric / segment parameters when deciding "strictly inside".
constexpr double INSIDE_EPSILON = 1e-12;
// Relative slack on power-distance comparisons.
constexpr double POWER_EPSILON = 1e-12;
// Same relative tolerance the mixed complex welds with.
constexpr double WELD_EPSILON = 1e-10;
// Condition 17 tolerance in units of the rounding scale eps * M / l.
constexpr double CONDITION_FACTOR = 1e6;
// Tets between stop checks and progress updates.
constexpr size_t CHECK_INTERVAL = 4096;

// A simplex as dimension << 30 | index into vertices()/edges()/triangles()/tets().
using Ref = uint32_t;
constexpr Ref NO_REF = UINT32_MAX;

Ref makeRef(uint32_t dimension, uint32_t index) {
    return (dimension << 30) | index;
}
uint32_t dimensionOf(Ref r) {
    return r >> 30;
}
uint32_t indexOf(Ref r) {
    return r & ((1u << 30) - 1);
}

uint64_t pairKey(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | b;
}

uint64_t tripleKey(std::array<uint32_t, 3> v) {
    std::sort(v.begin(), v.end());
    return (static_cast<uint64_t>(v[0]) << 42) | (static_cast<uint64_t>(v[1]) << 21) | v[2];
}

glm::dvec3 closestOnSegment(const glm::dvec3& p, const glm::dvec3& a, const glm::dvec3& b) {
    const glm::dvec3 ab = b - a;
    const double length2 = glm::dot(ab, ab);
    if (length2 == 0.0) {
        return a;
    }
    return a + std::clamp(glm::dot(p - a, ab) / length2, 0.0, 1.0) * ab;
}

// Ericson, Real-Time Collision Detection, 5.1.5.
glm::dvec3 closestOnTriangle(const glm::dvec3& p, const glm::dvec3& a, const glm::dvec3& b,
                             const glm::dvec3& c) {
    const glm::dvec3 ab = b - a;
    const glm::dvec3 ac = c - a;
    const glm::dvec3 ap = p - a;
    const double d1 = glm::dot(ab, ap);
    const double d2 = glm::dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        return a;
    }
    const glm::dvec3 bp = p - b;
    const double d3 = glm::dot(ab, bp);
    const double d4 = glm::dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) {
        return b;
    }
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        return a + (d1 / (d1 - d3)) * ab;
    }
    const glm::dvec3 cp = p - c;
    const double d5 = glm::dot(ab, cp);
    const double d6 = glm::dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) {
        return c;
    }
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        return a + (d2 / (d2 - d6)) * ac;
    }
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && d4 - d3 >= 0.0 && d5 - d6 >= 0.0) {
        return b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b);
    }
    const double denominator = 1.0 / (va + vb + vc);
    return a + (vb * denominator) * ab + (vc * denominator) * ac;
}

double distance2(const glm::dvec3& a, const glm::dvec3& b) {
    const glm::dvec3 d = a - b;
    return glm::dot(d, d);
}

// Anchors of the simplices of a power complex [KV07, section 2.4]: the
// Delaunay anchor of tau is the face of delta_tau whose centre is the point
// of delta_tau closest to c(tau) (Lemma 15); the Voronoi anchor of sigma is
// the coface whose Voronoi face holds the point of nu_sigma closest to
// c(sigma) (Lemma 16). Each is tau (sigma) itself if the centre lies strictly
// inside, else the anchor of the nearest facet, so both are computed in order
// of dimension. Delaunay anchors exist for simplices of input balls only,
// Voronoi anchors for simplices with a bounded dual.
class Anchors {
    public:
    Anchors(const PowerComplex& complex, size_t inputCount)
        : complex(complex), inputCount(inputCount) {
        indexSimplices();
        computeCentres();
        computeDelaunayAnchors();
        computeVoronoiAnchors();
    }

    bool isReal(Ref r) const {
        for (uint32_t v : verticesOf(r)) {
            if (complex.vertices()[v].inputIndex >= inputCount) {
                return false;
            }
        }
        return true;
    }

    Ref delaunay(Ref r) const { return delaunayAnchor[dimensionOf(r)][indexOf(r)]; }
    Ref voronoi(Ref r) const { return voronoiAnchor[dimensionOf(r)][indexOf(r)]; }
    const glm::dvec3& centre(Ref r) const { return centres[dimensionOf(r)][indexOf(r)]; }

    // The simplex spanned by a set of vertices of one tet.
    Ref simplexOf(const std::vector<uint32_t>& vertices, uint32_t tet) const {
        switch (vertices.size()) {
        case 1:
            return makeRef(0, vertices[0]);
        case 2:
            return makeRef(1, edgeIndex.at(pairKey(vertices[0], vertices[1])));
        case 3:
            return makeRef(2, triangleIndex.at(tripleKey({vertices[0], vertices[1], vertices[2]})));
        default:
            return makeRef(3, tet);
        }
    }

    std::vector<uint32_t> verticesOf(Ref r) const {
        const uint32_t i = indexOf(r);
        switch (dimensionOf(r)) {
        case 0:
            return {i};
        case 1:
            return {complex.edges()[i].v.begin(), complex.edges()[i].v.end()};
        case 2:
            return {complex.triangles()[i].v.begin(), complex.triangles()[i].v.end()};
        default:
            return {complex.tets()[i].v.begin(), complex.tets()[i].v.end()};
        }
    }

    // Lemma 15: c(delaunay anchor) is the point of delta closest to c(tau).
    double worstDelaunayError() const {
        double worst = 0.0;
        for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
            for (uint32_t i = 0; i < delaunayAnchor[dimension].size(); ++i) {
                const Ref r = makeRef(dimension, i);
                if (delaunay(r) == NO_REF) {
                    continue;
                }
                const glm::dvec3 closest = closestOnSimplex(r, centre(r));
                worst = std::max(worst, std::sqrt(distance2(closest, centre(delaunay(r)))));
            }
        }
        return worst / scale;
    }

    // Lemma 16: c(voronoi anchor) lies in nu_sigma, i.e. is no farther in power
    // distance from sigma's vertices than from any of their neighbours.
    double worstVoronoiViolation() const {
        double worst = 0.0;
        for (uint32_t dimension = 0; dimension <= 3; ++dimension) {
            for (uint32_t i = 0; i < voronoiAnchor[dimension].size(); ++i) {
                const Ref r = makeRef(dimension, i);
                if (voronoi(r) == NO_REF || !isReal(r)) {
                    continue;
                }
                const glm::dvec3& q = centre(voronoi(r));
                for (uint32_t v : verticesOf(r)) {
                    const double own = power(v, q);
                    for (uint32_t u : neighbours[v]) {
                        worst = std::max(worst, own - power(u, q));
                    }
                }
            }
        }
        return worst / (scale * scale);
    }

    // Counts of anchors that collapsed onto a proper face (coface), by dimension.
    std::array<uint32_t, 4> collapsedDelaunay() const { return collapsed(delaunayAnchor); }
    std::array<uint32_t, 4> collapsedVoronoi() const { return collapsed(voronoiAnchor); }

    private:
    const PowerComplex& complex;
    size_t inputCount;
    double scale = 1.0;
    double powerTolerance = 0.0;

    std::unordered_map<uint64_t, uint32_t> edgeIndex;
    std::unordered_map<uint64_t, uint32_t> triangleIndex;
    std::vector<std::vector<uint32_t>> neighbours;    // per vertex
    std::vector<std::vector<uint32_t>> incidentEdges; // per vertex

    std::array<std::vector<glm::dvec3>, 4> centres;
    std::array<std::vector<Ref>, 4> delaunayAnchor;
    std::array<std::vector<Ref>, 4> voronoiAnchor;

    double power(uint32_t v, const glm::dvec3& x) const {
        const DelaunayVertex& vertex = complex.vertices()[v];
        return distance2(vertex.position, x) - vertex.weight;
    }

    void indexSimplices() {
        const auto& vertices = complex.vertices();
        neighbours.resize(vertices.size());
        incidentEdges.resize(vertices.size());
        double extent = 0.0;
        double weights = 0.0;
        for (const DelaunayVertex& v : vertices) {
            const glm::dvec3 a = glm::abs(v.position);
            extent = std::max({extent, a.x, a.y, a.z});
            weights = std::max(weights, std::abs(v.weight));
        }
        scale = 1.0 + extent + std::sqrt(weights);
        powerTolerance = POWER_EPSILON * scale * scale;

        for (uint32_t e = 0; e < complex.edges().size(); ++e) {
            const auto& [a, b] = complex.edges()[e].v;
            edgeIndex.emplace(pairKey(a, b), e);
            neighbours[a].push_back(b);
            neighbours[b].push_back(a);
            incidentEdges[a].push_back(e);
            incidentEdges[b].push_back(e);
        }
        for (uint32_t t = 0; t < complex.triangles().size(); ++t) {
            triangleIndex.emplace(tripleKey(complex.triangles()[t].v), t);
        }
    }

    // Weighted circumcentres: in aff(delta), equal power distance to every
    // vertex. Edge: closed form. Triangle: its Voronoi edge is perpendicular
    // to aff(delta) and passes through the centre, so project one of its
    // Voronoi vertices (CGAL's, correctly rounded) onto the plane; solving the
    // normal equations instead squares the condition number, which cost
    // ~1e-7 on triangles reaching the bounding octahedron. Tet: the Voronoi
    // vertex.
    glm::dvec3 edgeCentre(const DelaunayEdge& e) const {
        const DelaunayVertex& p0 = complex.vertices()[e.v[0]];
        const DelaunayVertex& p1 = complex.vertices()[e.v[1]];
        const glm::dvec3 d = p1.position - p0.position;
        const double length2 = glm::dot(d, d);
        return p0.position + ((length2 - p1.weight + p0.weight) / (2.0 * length2)) * d;
    }

    glm::dvec3 triangleCentre(uint32_t t) const {
        const auto& v = complex.triangles()[t].v;
        const glm::dvec3& a = position(v[0]);
        const glm::dvec3 n = glm::normalize(glm::cross(position(v[1]) - a, position(v[2]) - a));
        const glm::dvec3& y = complex.voronoiVertices()[complex.voronoiEdges()[t].tets[0]];
        return y - glm::dot(y - a, n) * n;
    }

    void computeCentres() {
        for (const DelaunayVertex& v : complex.vertices()) {
            centres[0].push_back(v.position);
        }
        for (const DelaunayEdge& e : complex.edges()) {
            centres[1].push_back(edgeCentre(e));
        }
        for (uint32_t t = 0; t < complex.triangles().size(); ++t) {
            centres[2].push_back(triangleCentre(t));
        }
        centres[3] = complex.voronoiVertices();
    }

    glm::dvec3 position(uint32_t v) const { return complex.vertices()[v].position; }

    // Barycentric coordinates of x (in aff of the simplex), all > eps.
    bool strictlyInside(Ref r, const glm::dvec3& x) const {
        const std::vector<uint32_t> v = verticesOf(r);
        if (v.size() == 2) {
            const glm::dvec3 e = position(v[1]) - position(v[0]);
            const double t = glm::dot(x - position(v[0]), e) / glm::dot(e, e);
            return t > INSIDE_EPSILON && t < 1.0 - INSIDE_EPSILON;
        }
        if (v.size() == 3) {
            const glm::dvec3 e1 = position(v[1]) - position(v[0]);
            const glm::dvec3 e2 = position(v[2]) - position(v[0]);
            const glm::dvec3 d = x - position(v[0]);
            const double g11 = glm::dot(e1, e1);
            const double g12 = glm::dot(e1, e2);
            const double g22 = glm::dot(e2, e2);
            const double r1 = glm::dot(d, e1);
            const double r2 = glm::dot(d, e2);
            const double determinant = g11 * g22 - g12 * g12;
            const double a1 = (r1 * g22 - r2 * g12) / determinant;
            const double a2 = (g11 * r2 - g12 * r1) / determinant;
            return a1 > INSIDE_EPSILON && a2 > INSIDE_EPSILON &&
                   1.0 - a1 - a2 > INSIDE_EPSILON;
        }
        const glm::dmat3 m(position(v[1]) - position(v[0]), position(v[2]) - position(v[0]),
                           position(v[3]) - position(v[0]));
        const glm::dvec3 a = glm::inverse(m) * (x - position(v[0]));
        return a.x > INSIDE_EPSILON && a.y > INSIDE_EPSILON && a.z > INSIDE_EPSILON &&
               1.0 - a.x - a.y - a.z > INSIDE_EPSILON;
    }

    // Facets of a simplex as Refs, with their vertex lists.
    std::vector<Ref> facetsOf(Ref r) const {
        const std::vector<uint32_t> v = verticesOf(r);
        std::vector<Ref> facets;
        for (size_t skip = 0; skip < v.size(); ++skip) {
            std::vector<uint32_t> rest;
            for (size_t k = 0; k < v.size(); ++k) {
                if (k != skip) {
                    rest.push_back(v[k]);
                }
            }
            facets.push_back(simplexOf(rest, indexOf(r)));
        }
        return facets;
    }

    glm::dvec3 closestOnSimplex(Ref r, const glm::dvec3& x) const {
        const std::vector<uint32_t> v = verticesOf(r);
        switch (v.size()) {
        case 1:
            return position(v[0]);
        case 2:
            return closestOnSegment(x, position(v[0]), position(v[1]));
        case 3:
            return closestOnTriangle(x, position(v[0]), position(v[1]), position(v[2]));
        default:
            if (strictlyInside(r, x)) {
                return x;
            }
            glm::dvec3 best = position(v[0]);
            for (Ref facet : facetsOf(r)) {
                const glm::dvec3 candidate = closestOnSimplex(facet, x);
                if (distance2(candidate, x) < distance2(best, x)) {
                    best = candidate;
                }
            }
            return best;
        }
    }

    void computeDelaunayAnchors() {
        for (uint32_t dimension = 0; dimension <= 3; ++dimension) {
            delaunayAnchor[dimension].assign(centres[dimension].size(), NO_REF);
            for (uint32_t i = 0; i < centres[dimension].size(); ++i) {
                const Ref r = makeRef(dimension, i);
                if (!isReal(r)) {
                    continue;
                }
                if (dimension == 0 || strictlyInside(r, centre(r))) {
                    delaunayAnchor[dimension][i] = r;
                    continue;
                }
                Ref nearest = NO_REF;
                double best = std::numeric_limits<double>::infinity();
                for (Ref facet : facetsOf(r)) {
                    const double d = distance2(closestOnSimplex(facet, centre(r)), centre(r));
                    if (d < best) {
                        best = d;
                        nearest = facet;
                    }
                }
                delaunayAnchor[dimension][i] = delaunay(nearest);
            }
        }
    }

    // The triangle shared by two tets that both contain edge (a, b).
    uint32_t sharedTriangle(uint32_t a, uint32_t b, uint32_t tet0, uint32_t tet1) const {
        for (uint32_t x : complex.tets()[tet0].v) {
            if (x == a || x == b) {
                continue;
            }
            const auto& other = complex.tets()[tet1].v;
            if (std::find(other.begin(), other.end(), x) != other.end()) {
                return triangleIndex.at(tripleKey({a, b, x}));
            }
        }
        return UINT32_MAX;
    }

    void computeVoronoiAnchors() {
        const auto& tets = complex.tets();
        const auto& voronoiVertices = complex.voronoiVertices();
        for (uint32_t dimension = 0; dimension <= 3; ++dimension) {
            voronoiAnchor[dimension].assign(centres[dimension].size(), NO_REF);
        }
        for (uint32_t c = 0; c < tets.size(); ++c) {
            voronoiAnchor[3][c] = makeRef(3, c);
        }

        // Triangle: nu is the Voronoi edge between its two tets' vertices.
        for (uint32_t t = 0; t < complex.triangles().size(); ++t) {
            const VoronoiEdge& edge = complex.voronoiEdges()[t];
            if (edge.tets[1] == kInfinite) {
                continue;
            }
            const glm::dvec3& y0 = voronoiVertices[edge.tets[0]];
            const glm::dvec3& y1 = voronoiVertices[edge.tets[1]];
            const glm::dvec3 u = y1 - y0;
            const double length2 = glm::dot(u, u);
            const double param =
                length2 > 0.0 ? glm::dot(centres[2][t] - y0, u) / length2 : 0.0;
            if (param > INSIDE_EPSILON && param < 1.0 - INSIDE_EPSILON) {
                voronoiAnchor[2][t] = makeRef(2, t);
            } else {
                voronoiAnchor[2][t] = makeRef(3, param < 0.5 ? edge.tets[0] : edge.tets[1]);
            }
        }

        // Edge: nu is a polygon of Voronoi vertices; its sides are the Voronoi
        // edges of the triangles around the edge.
        for (uint32_t e = 0; e < complex.edges().size(); ++e) {
            const VoronoiFace& face = complex.voronoiFaces()[e];
            if (!face.bounded) {
                continue;
            }
            const auto& [a, b] = complex.edges()[e].v;
            const glm::dvec3& c = centres[1][e];
            const double own = power(a, c);
            const size_t n = face.ring.size();
            bool inside = true;
            Ref nearest = NO_REF;
            double best = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < n; ++k) {
                const uint32_t t = sharedTriangle(a, b, face.ring[k], face.ring[(k + 1) % n]);
                const auto& tv = complex.triangles()[t].v;
                const uint32_t third = tv[0] != a && tv[0] != b ? tv[0]
                                       : tv[1] != a && tv[1] != b ? tv[1]
                                                                  : tv[2];
                inside = inside && power(third, c) - own > powerTolerance;
                const double d = distance2(
                    closestOnSegment(c, voronoiVertices[face.ring[k]],
                                     voronoiVertices[face.ring[(k + 1) % n]]),
                    c);
                if (d < best) {
                    best = d;
                    nearest = makeRef(2, t);
                }
            }
            voronoiAnchor[1][e] = inside ? makeRef(1, e) : voronoi(nearest);
        }

        // Vertex: nu is the power cell; its facets are the Voronoi faces of the
        // incident edges.
        for (uint32_t v = 0; v < complex.vertices().size(); ++v) {
            if (!complex.voronoiCells()[v].bounded) {
                continue;
            }
            const glm::dvec3& p = position(v);
            const double own = -complex.vertices()[v].weight;
            bool inside = true;
            for (uint32_t u : neighbours[v]) {
                inside = inside && power(u, p) - own > powerTolerance;
            }
            if (inside) {
                voronoiAnchor[0][v] = makeRef(0, v);
                continue;
            }
            Ref nearest = NO_REF;
            double best = std::numeric_limits<double>::infinity();
            for (uint32_t e : incidentEdges[v]) {
                const auto& ring = complex.voronoiFaces()[e].ring;
                for (size_t k = 1; k + 1 < ring.size(); ++k) {
                    const double d = distance2(
                        closestOnTriangle(p, voronoiVertices[ring[0]], voronoiVertices[ring[k]],
                                          voronoiVertices[ring[k + 1]]),
                        p);
                    if (d < best) {
                        best = d;
                        nearest = makeRef(1, e);
                    }
                }
            }
            voronoiAnchor[0][v] = voronoi(nearest);
        }
    }

    static std::array<uint32_t, 4> collapsed(const std::array<std::vector<Ref>, 4>& anchors) {
        std::array<uint32_t, 4> counts{};
        for (uint32_t dimension = 0; dimension <= 3; ++dimension) {
            for (uint32_t i = 0; i < anchors[dimension].size(); ++i) {
                const Ref a = anchors[dimension][i];
                if (a != NO_REF && a != makeRef(dimension, i)) {
                    ++counts[dimension];
                }
            }
        }
        return counts;
    }
};

} // namespace

std::vector<WeightedPoint> boundingOctahedron(std::span<const WeightedPoint> points, double s) {
    if (points.empty()) {
        return {};
    }
    glm::dvec3 lo(std::numeric_limits<double>::infinity());
    glm::dvec3 hi(-std::numeric_limits<double>::infinity());
    double maxWeight = 0.0;
    for (const WeightedPoint& point : points) {
        lo = glm::min(lo, glm::dvec3(point.position));
        hi = glm::max(hi, glm::dvec3(point.position));
        maxWeight = std::max(maxWeight, point.weight);
    }
    const glm::dvec3 extent = hi - lo;
    const glm::dvec3 mid = lo + 0.5 * extent;
    const double dr = (extent.x + extent.y + extent.z + std::sqrt(maxWeight) + 0.001) / s;

    std::vector<WeightedPoint> octahedron;
    for (int axis = 0; axis < 3; ++axis) {
        for (const double sign : {1.0, -1.0}) {
            glm::dvec3 p = mid;
            p[axis] += sign * dr;
            octahedron.emplace_back(glm::vec3(p), -1.0);
        }
    }
    return octahedron;
}

KvTetComplex buildKvTetComplex(std::span<const WeightedPoint> points, double s) {
    KvTetComplex result;
    if (points.empty()) {
        return result;
    }
    std::vector<WeightedPoint> all(points.begin(), points.end());
    const std::vector<WeightedPoint> octahedron = boundingOctahedron(points, s);
    all.insert(all.end(), octahedron.begin(), octahedron.end());

    PowerComplex complex;
    complex.build(all);
    const Anchors anchors(complex, points.size());
    const auto& tets = complex.tets();
    const auto inputsOf = [&](Ref r) {
        std::vector<uint32_t> inputs;
        for (uint32_t v : anchors.verticesOf(r)) {
            inputs.push_back(complex.vertices()[v].inputIndex);
        }
        return inputs;
    };

    // Cells: mixed cells of simplices of input balls.
    std::unordered_map<Ref, uint32_t> cellOf;
    const std::array<size_t, 4> counts = {complex.vertices().size(), complex.edges().size(),
                                          complex.triangles().size(), tets.size()};
    for (uint32_t dimension = 0; dimension <= 3; ++dimension) {
        for (uint32_t i = 0; i < counts[dimension]; ++i) {
            const Ref r = makeRef(dimension, i);
            if (anchors.isReal(r)) {
                cellOf.emplace(r, static_cast<uint32_t>(result.cells.size()));
                result.cells.push_back({.dimension = dimension, .simplex = i});
                result.cellInputs.push_back(inputsOf(r));
                result.quadrics.push_back(makeSkinQuadric(complex, dimension, i, s));
            }
        }
    }

    // Anchor points of every polyhedral cell mu_{tau, sigma}, tau a simplex of
    // input balls, tau <= sigma. Every such pair lies in some tet.
    double extent = 0.0;
    for (const DelaunayVertex& v : complex.vertices()) {
        const glm::dvec3 a = glm::abs(v.position);
        extent = std::max({extent, a.x, a.y, a.z});
    }
    PointWelder welder(WELD_EPSILON * (1.0 + extent));
    std::unordered_map<uint64_t, uint32_t> anchorPoint;
    double largestCentre = 0.0;
    const auto magnitude = [](const glm::dvec3& p) {
        return std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)});
    };
    const auto key = [&](Ref tau, Ref sigma) {
        return (static_cast<uint64_t>(anchors.delaunay(tau)) << 32) | anchors.voronoi(sigma);
    };
    const auto subset = [&](uint32_t c, uint32_t mask) {
        std::vector<uint32_t> vertices;
        for (uint32_t k = 0; k < 4; ++k) {
            if (mask & (1u << k)) {
                vertices.push_back(tets[c].v[k]);
            }
        }
        return anchors.simplexOf(vertices, c);
    };
    for (uint32_t c = 0; c < tets.size(); ++c) {
        for (uint32_t tauMask = 1; tauMask < 16; ++tauMask) {
            const Ref tau = subset(c, tauMask);
            if (!anchors.isReal(tau)) {
                continue;
            }
            for (uint32_t sigmaMask = tauMask; sigmaMask < 16; ++sigmaMask) {
                if ((sigmaMask & tauMask) != tauMask) {
                    continue;
                }
                const Ref sigma = subset(c, sigmaMask);
                const uint64_t k = key(tau, sigma);
                if (!anchorPoint.contains(k)) {
                    const glm::dvec3& cDel = anchors.centre(anchors.delaunay(tau));
                    const glm::dvec3& cVor = anchors.centre(anchors.voronoi(sigma));
                    largestCentre = std::max({largestCentre, magnitude(cDel), magnitude(cVor)});
                    const uint32_t id = welder.idOf((1.0 - s) * cDel + s * cVor);
                    anchorPoint.emplace(k, id);
                    if (id == result.pointLabels.size()) {
                        result.pointLabels.push_back({.delaunay = inputsOf(anchors.delaunay(tau)),
                                                      .voronoi = inputsOf(anchors.voronoi(sigma))});
                    }
                }
            }
        }
    }
    result.anchorPairs = anchorPoint.size();
    result.points = welder.takePoints();
    result.weldedPairs = result.anchorPairs - result.points.size();

    // The tets [KV07, section 3.2], built from flags of polyhedral cells as in
    // CGAL's Mixed_complex_triangulator_3; (tau, sigma) names the anchor of
    // mu_{tau, sigma}. A flag whose anchors coincide spans no tet.
    const auto anchor = [&](Ref tau, Ref sigma) { return anchorPoint.at(key(tau, sigma)); };
    const auto addTet = [&](std::array<uint32_t, 4> tet, Ref cell) {
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = i + 1; j < 4; ++j) {
                if (tet[i] == tet[j]) {
                    ++result.skippedTets;
                    return;
                }
            }
        }
        result.tets.push_back(tet);
        result.tetCells.push_back(cellOf.at(cell));
    };
    const auto vertexRef = [](uint32_t v) { return makeRef(0, v); };
    const auto edgeRef = [&](uint32_t a, uint32_t b, uint32_t c) {
        return anchors.simplexOf({a, b}, c);
    };
    const auto triangleRef = [&](uint32_t a, uint32_t b, uint32_t d, uint32_t c) {
        return anchors.simplexOf({a, b, d}, c);
    };
    const auto others = [](const std::array<uint32_t, 4>& v, std::initializer_list<uint32_t> skip) {
        std::vector<uint32_t> rest;
        for (uint32_t x : v) {
            if (std::find(skip.begin(), skip.end(), x) == skip.end()) {
                rest.push_back(x);
            }
        }
        return rest;
    };

    std::vector<std::vector<uint32_t>> tetsOfVertex(complex.vertices().size());
    for (uint32_t c = 0; c < tets.size(); ++c) {
        for (uint32_t v : tets[c].v) {
            tetsOfVertex[v].push_back(c);
        }
    }

    // Vertex cells: flags v < e < f < c.
    for (uint32_t v = 0; v < complex.vertices().size(); ++v) {
        const Ref vr = vertexRef(v);
        if (!anchors.isReal(vr)) {
            continue;
        }
        for (uint32_t c : tetsOfVertex[v]) {
            const Ref cr = makeRef(3, c);
            const std::vector<uint32_t> rest = others(tets[c].v, {v});
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    if (i == j) {
                        continue;
                    }
                    const Ref fr = triangleRef(v, rest[i], rest[j], c);
                    const Ref er = edgeRef(v, rest[i], c);
                    addTet({anchor(vr, vr), anchor(vr, er), anchor(vr, fr), anchor(vr, cr)}, vr);
                }
            }
        }
    }

    // Edge cells: on each side v of the edge, for each tet c and facet f
    // around it.
    for (uint32_t e = 0; e < complex.edges().size(); ++e) {
        const Ref er = makeRef(1, e);
        if (!anchors.isReal(er)) {
            continue;
        }
        const auto& ring = complex.voronoiFaces()[e].ring;
        for (uint32_t endpoint : complex.edges()[e].v) {
            const Ref vr = vertexRef(endpoint);
            if (anchors.delaunay(vr) == anchors.delaunay(er)) {
                continue;
            }
            for (uint32_t c : ring) {
                const Ref cr = makeRef(3, c);
                const auto& [a, b] = complex.edges()[e].v;
                for (uint32_t x : others(tets[c].v, {a, b})) {
                    const Ref fr = triangleRef(a, b, x, c);
                    if (anchors.voronoi(cr) == anchors.voronoi(fr) ||
                        anchors.voronoi(fr) == anchors.voronoi(er)) {
                        continue;
                    }
                    addTet({anchor(vr, er), anchor(er, er), anchor(er, fr), anchor(er, cr)}, er);
                    addTet({anchor(vr, er), anchor(vr, fr), anchor(er, fr), anchor(er, cr)}, er);
                    addTet({anchor(vr, er), anchor(vr, fr), anchor(vr, cr), anchor(er, cr)}, er);
                }
            }
        }
    }

    // Triangle cells: on each side c of the triangle, for each vertex v and
    // edge e of it through v.
    for (uint32_t t = 0; t < complex.triangles().size(); ++t) {
        const Ref fr = makeRef(2, t);
        if (!anchors.isReal(fr)) {
            continue;
        }
        const auto& tv = complex.triangles()[t].v;
        for (uint32_t c : complex.voronoiEdges()[t].tets) {
            if (c == kInfinite) {
                continue;
            }
            const Ref cr = makeRef(3, c);
            if (anchor(fr, fr) == anchor(fr, cr)) {
                continue;
            }
            for (size_t i = 0; i < 3; ++i) {
                const Ref vr = vertexRef(tv[i]);
                for (size_t j = 0; j < 3; ++j) {
                    if (i == j) {
                        continue;
                    }
                    const Ref er = edgeRef(tv[i], tv[j], c);
                    if (anchor(vr, fr) == anchor(er, fr) || anchor(er, fr) == anchor(fr, fr)) {
                        continue;
                    }
                    addTet({anchor(vr, fr), anchor(er, fr), anchor(fr, fr), anchor(fr, cr)}, fr);
                    addTet({anchor(vr, fr), anchor(er, fr), anchor(er, cr), anchor(fr, cr)}, fr);
                    addTet({anchor(vr, fr), anchor(vr, cr), anchor(er, cr), anchor(fr, cr)}, fr);
                }
            }
        }
    }

    // Tet cells: flags c > f > e > v.
    for (uint32_t c = 0; c < tets.size(); ++c) {
        const Ref cr = makeRef(3, c);
        if (!anchors.isReal(cr)) {
            continue;
        }
        const auto& v = tets[c].v;
        for (size_t opposite = 0; opposite < 4; ++opposite) {
            const std::vector<uint32_t> fv = others(v, {v[opposite]});
            const Ref fr = triangleRef(fv[0], fv[1], fv[2], c);
            if (anchor(cr, cr) == anchor(fr, cr)) {
                continue;
            }
            for (size_t i = 0; i < 3; ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    if (i == j) {
                        continue;
                    }
                    const Ref er = edgeRef(fv[i], fv[j], c);
                    const Ref vr = vertexRef(fv[i]);
                    if (anchor(fr, cr) == anchor(er, cr) || anchor(er, cr) == anchor(vr, cr)) {
                        continue;
                    }
                    addTet({anchor(cr, cr), anchor(fr, cr), anchor(er, cr), anchor(vr, cr)}, cr);
                }
            }
        }
    }

    // Condition 17 on every tet edge ab, oriented so F(a) <= F(b): |P d| (the
    // distance to aff nu) non-increasing, |(I - P) d| non-decreasing. Both are
    // quadratic along the edge, so checking their slopes at a and b suffices.
    // Where an endpoint is the anchor of a face holding the edge the exact
    // slope is 0 (first-order optimality), so the tolerance follows rounding.
    glm::dvec3 lo(std::numeric_limits<double>::infinity());
    glm::dvec3 hi(-std::numeric_limits<double>::infinity());
    double largestRadius = 0.0;
    for (const WeightedPoint& point : points) {
        lo = glm::min(lo, glm::dvec3(point.position));
        hi = glm::max(hi, glm::dvec3(point.position));
        largestRadius = std::max(largestRadius, std::sqrt(std::max(point.weight, 0.0)));
    }
    const double inputExtent = std::max(glm::length(hi - lo), 2.0 * largestRadius);
    result.roundingScale = inputExtent > 0.0 ? std::numeric_limits<double>::epsilon() *
                                                   largestCentre / inputExtent
                                             : 0.0;
    const double conditionTolerance = CONDITION_FACTOR * result.roundingScale;
    const glm::dmat3 identity(1.0);
    for (size_t t = 0; t < result.tets.size(); ++t) {
        const SkinQuadric& q = result.quadrics[result.tetCells[t]];
        const auto& tet = result.tets[t];
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = i + 1; j < 4; ++j) {
                glm::dvec3 a = result.points[tet[i]];
                glm::dvec3 b = result.points[tet[j]];
                if (q.value(a) > q.value(b)) {
                    std::swap(a, b);
                }
                const glm::dvec3 e = b - a;
                const glm::dvec3 d0 = a - q.centre;
                const glm::dvec3 d1 = b - q.centre;
                const glm::dvec3 pe = q.projector * e;
                const glm::dvec3 qe = (identity - q.projector) * e;
                const double norm = glm::length(e) * (glm::length(d0) + glm::length(e));
                if (norm == 0.0) {
                    continue;
                }
                const double violation =
                    std::max({glm::dot(q.projector * d0, pe), glm::dot(q.projector * d1, pe),
                              -glm::dot((identity - q.projector) * d0, qe),
                              -glm::dot((identity - q.projector) * d1, qe)}) /
                    norm;
                if (violation > conditionTolerance) {
                    ++result.monotonicityViolations;
                }
                result.worstMonotonicity = std::max(result.worstMonotonicity, violation);
            }
        }
    }

    // Tiling: delta_X and nu_X are orthogonal and complementary, so
    // vol(mu_X) = (1 - s)^k s^(3 - k) vol_k(delta_X) vol_(3-k)(nu_X), k = dim X.
    std::vector<double> tetVolume(result.cells.size(), 0.0);
    for (size_t t = 0; t < result.tets.size(); ++t) {
        const auto& [i0, i1, i2, i3] = result.tets[t];
        const glm::dvec3& p0 = result.points[i0];
        tetVolume[result.tetCells[t]] +=
            std::abs(glm::dot(result.points[i1] - p0, glm::cross(result.points[i2] - p0,
                                                                 result.points[i3] - p0))) /
            6.0;
    }
    const auto& voronoiVertices = complex.voronoiVertices();
    const auto polygonArea = [&](const std::vector<uint32_t>& ring) {
        glm::dvec3 normal(0.0);
        for (size_t k = 1; k + 1 < ring.size(); ++k) {
            normal += glm::cross(voronoiVertices[ring[k]] - voronoiVertices[ring[0]],
                                 voronoiVertices[ring[k + 1]] - voronoiVertices[ring[0]]);
        }
        return 0.5 * glm::length(normal);
    };
    for (size_t i = 0; i < result.cells.size(); ++i) {
        const KvTetComplex::Cell& cell = result.cells[i];
        double expected = 0.0;
        switch (cell.dimension) {
        case 0: {
            // Cone the power cell from its vertex mean over its fanned faces.
            const auto& faces = complex.voronoiCells()[cell.simplex].faces;
            glm::dvec3 mean(0.0);
            size_t count = 0;
            for (uint32_t e : faces) {
                for (uint32_t y : complex.voronoiFaces()[e].ring) {
                    mean += voronoiVertices[y];
                    ++count;
                }
            }
            mean /= static_cast<double>(count);
            double volume = 0.0;
            for (uint32_t e : faces) {
                const auto& ring = complex.voronoiFaces()[e].ring;
                for (size_t k = 1; k + 1 < ring.size(); ++k) {
                    volume += std::abs(glm::dot(
                                  voronoiVertices[ring[0]] - mean,
                                  glm::cross(voronoiVertices[ring[k]] - mean,
                                             voronoiVertices[ring[k + 1]] - mean))) /
                              6.0;
                }
            }
            expected = s * s * s * volume;
            break;
        }
        case 1: {
            const auto& [a, b] = complex.edges()[cell.simplex].v;
            const double length = glm::length(complex.vertices()[b].position -
                                              complex.vertices()[a].position);
            expected = (1.0 - s) * s * s * length *
                       polygonArea(complex.voronoiFaces()[cell.simplex].ring);
            break;
        }
        case 2: {
            const auto& v = complex.triangles()[cell.simplex].v;
            const glm::dvec3& p0 = complex.vertices()[v[0]].position;
            const double area = 0.5 * glm::length(glm::cross(
                                          complex.vertices()[v[1]].position - p0,
                                          complex.vertices()[v[2]].position - p0));
            const auto& [y0, y1] = complex.voronoiEdges()[cell.simplex].tets;
            const double length = glm::length(voronoiVertices[y1] - voronoiVertices[y0]);
            expected = (1.0 - s) * (1.0 - s) * s * area * length;
            break;
        }
        default: {
            const auto& v = complex.tets()[cell.simplex].v;
            const glm::dvec3& p0 = complex.vertices()[v[0]].position;
            const double volume =
                std::abs(glm::dot(complex.vertices()[v[1]].position - p0,
                                  glm::cross(complex.vertices()[v[2]].position - p0,
                                             complex.vertices()[v[3]].position - p0))) /
                6.0;
            expected = (1.0 - s) * (1.0 - s) * (1.0 - s) * volume;
            break;
        }
        }
        if (expected > 0.0) {
            result.worstTilingError = std::max(
                result.worstTilingError, std::abs(tetVolume[i] - expected) / expected);
        }
    }

    const auto del = anchors.collapsedDelaunay();
    const auto vor = anchors.collapsedVoronoi();
    const nacre::core::Logger logger{"KruithofVegter"};
    logger.print("{} cells; anchors: {} pairs -> {} points ({} welded); Delaunay anchors on a "
                 "face: {} E / {} F / {} T; Voronoi anchors on a coface: {} V / {} E / {} F",
                 result.cells.size(), result.anchorPairs, result.points.size(),
                 result.weldedPairs, del[1], del[2], del[3], vor[0], vor[1], vor[2]);
    logger.print("Lemma 15 check (closest point vs anchor centre): {:.3e}; Lemma 16 check "
                 "(anchor outside its Voronoi face): {:.3e}",
                 anchors.worstDelaunayError(), anchors.worstVoronoiViolation());
    logger.print("{} tets ({} degenerate flags skipped); per-cell tiling error {:.3e}; "
                 "Condition 17: {} edge violations, worst slope {:.3e} = {:.3g} x rounding "
                 "scale {:.3e} (tolerance {:.0e} x)",
                 result.tets.size(), result.skippedTets, result.worstTilingError,
                 result.monotonicityViolations, result.worstMonotonicity,
                 result.roundingScale > 0.0 ? result.worstMonotonicity / result.roundingScale
                                            : 0.0,
                 result.roundingScale, CONDITION_FACTOR);
    if (result.monotonicityViolations > 0) {
        logger.warn("Monotonicity condition violated; the isotopy guarantee does not apply");
    }
    return result;
}

KvSkinResult meshSkinKruithofVegter(std::span<const WeightedPoint> points, double s,
                                    MeshingControl* control) {
    KvSkinResult result;
    const auto stopped = [&] { return control != nullptr && control->stopRequested(); };
    if (control != nullptr) {
        control->phase = MeshingPhase::Triangulating;
        control->fraction = -1.0f;
    }
    const KvTetComplex complex = buildKvTetComplex(points, s);
    if (stopped()) {
        result.cancelled = true;
        return result;
    }
    result.tets = complex.tets.size();
    result.monotonicityViolations = complex.monotonicityViolations;

    std::vector<double> values(complex.points.size(), std::numeric_limits<double>::quiet_NaN());
    std::vector<uint8_t> dimensions;
    dimensions.reserve(complex.cells.size());
    for (const KvTetComplex::Cell& cell : complex.cells) {
        dimensions.push_back(static_cast<uint8_t>(cell.dimension));
    }
    TetMarcher marcher(complex.points, values, complex.quadrics, std::move(dimensions));
    if (control != nullptr) {
        control->phase = MeshingPhase::Marching;
        control->tets = complex.tets.size();
    }
    for (size_t t = 0; t < complex.tets.size(); ++t) {
        if (t % CHECK_INTERVAL == 0) {
            if (stopped()) {
                result.cancelled = true;
                return result;
            }
            if (control != nullptr) {
                control->fraction =
                    static_cast<float>(t) / static_cast<float>(complex.tets.size());
            }
        }
        marcher.march(complex.tets[t], complex.tetCells[t]);
    }
    result.mesh = marcher.finish();
    return result;
}

} // namespace skin::geometry
