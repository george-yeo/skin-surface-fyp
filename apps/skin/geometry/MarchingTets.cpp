#include "skin/geometry/MarchingTets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

#include "skin/geometry/MixedComplex.hpp"
#include "skin/geometry/SkinBuild.hpp"
#include "skin/geometry/SkinQuadric.hpp"
#include "skin/geometry/TetMarcher.hpp"

namespace skin::geometry {

namespace {

// Patch radii are floored at this fraction of the median patch radius:
// w_X ~ 0 is a cone point, and resolving it fully costs the whole budget.
constexpr double MIN_RELATIVE_SCALE = 0.1;
// Iterations between stop checks and progress updates.
constexpr size_t CHECK_INTERVAL = 4096;
constexpr size_t MAX_TETS = 4'000'000;
// Rehashing the edge map dominated refinement; ~8 MB of buckets up front.
constexpr size_t EDGE_RESERVE = 1u << 20;

using Tet = std::array<uint32_t, 4>;

uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | b;
}

double longestEdge(const std::array<glm::dvec3, 4>& p) {
    double longest = 0.0;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            longest = std::max(longest, glm::length(p[j] - p[i]));
        }
    }
    return longest;
}

std::vector<uint8_t> cellDimensions(const MixedComplex& mixed) {
    std::vector<uint8_t> dimensions;
    dimensions.reserve(mixed.cells.size());
    for (const MixedCell& cell : mixed.cells) {
        dimensions.push_back(static_cast<uint8_t>(cell.dimension));
    }
    return dimensions;
}

class Mesher {
    public:
    Mesher(const MixedComplex& mixed, const MixedComplexTopology& topology,
           const std::vector<SkinQuadric>& quadrics)
        : quadrics(quadrics), points(topology.points),
          marcher(points, values, quadrics, cellDimensions(mixed)) {
        values.assign(points.size(), std::numeric_limits<double>::quiet_NaN());
    }

    MarchingTetsResult run(const MixedComplexTopology& topology, double k,
                           MeshingControl* control) {
        MarchingTetsResult result;
        const auto stopped = [&] { return control != nullptr && control->stopRequested(); };
        if (control != nullptr) {
            control->phase = MeshingPhase::Refining;
            control->fraction = -1.0f;
        }

        std::vector<double> scales;
        for (const SkinQuadric& quadric : quadrics) {
            if (quadric.scale > 0.0) {
                scales.push_back(quadric.scale);
            }
        }
        if (scales.empty()) {
            return result;
        }
        std::nth_element(scales.begin(), scales.begin() + scales.size() / 2, scales.end());
        const double minScale = MIN_RELATIVE_SCALE * scales[scales.size() / 2];
        targets.reserve(quadrics.size());
        for (const SkinQuadric& quadric : quadrics) {
            targets.push_back(k * std::max(quadric.scale, minScale));
        }

        edgeLists.reserve(EDGE_RESERVE);
        coneCells(topology);
        for (size_t step = 0; !pending.empty(); ++step) {
            if (step % CHECK_INTERVAL == 0) {
                if (stopped()) {
                    result.cancelled = true;
                    return result;
                }
                if (control != nullptr) {
                    control->tets = tets.size();
                }
            }
            const uint32_t t = pending.back();
            pending.pop_back();
            if (!alive[t] || !needsRefinement(t)) {
                continue;
            }
            if (tets.size() >= MAX_TETS) {
                budgetExceeded = true;
                break;
            }
            bisect(longestEdgeKey(t));
        }

        if (control != nullptr) {
            control->phase = MeshingPhase::Marching;
            control->tets = tets.size();
        }
        for (uint32_t t = 0; t < tets.size(); ++t) {
            if (t % CHECK_INTERVAL == 0) {
                if (stopped()) {
                    result.cancelled = true;
                    return result;
                }
                if (control != nullptr) {
                    control->fraction = static_cast<float>(t) / static_cast<float>(tets.size());
                }
            }
            if (alive[t]) {
                ++result.tets;
                marcher.march(tets[t], tetCells[t]);
            }
        }
        result.mesh = marcher.finish();
        result.budgetExceeded = budgetExceeded;
        return result;
    }

    private:
    const std::vector<SkinQuadric>& quadrics;
    std::vector<double> targets; // per cell: k * patch radius
    std::vector<glm::dvec3> points;
    std::vector<double> values; // F, evaluated once per point
    TetMarcher marcher;         // after points and values: holds references to them
    std::unordered_map<uint64_t, uint32_t> midpoints;

    std::vector<Tet> tets;
    std::vector<uint32_t> tetCells;
    std::vector<bool> alive;
    // Tets holding each edge, as intrusive lists: tet t's edge k is slot 6t + k,
    // edgeNext links slots holding the same edge in insertion order, and the
    // map only stores each list's ends. Lists keep dead tets.
    struct EdgeList {
        uint32_t head;
        uint32_t tail;
    };
    static constexpr uint32_t NO_SLOT = UINT32_MAX;
    std::unordered_map<uint64_t, EdgeList> edgeLists;
    std::vector<uint32_t> edgeNext;
    std::vector<uint32_t> pending;
    bool budgetExceeded = false;

    void coneCells(const MixedComplexTopology& topology) {
        for (uint32_t c = 0; c < topology.cellFaces.size(); ++c) {
            const auto& faces = topology.cellFaces[c];
            std::vector<uint32_t> ids;
            for (const auto& ring : faces) {
                ids.insert(ids.end(), ring.begin(), ring.end());
            }
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            if (ids.size() < 4) {
                continue;
            }
            glm::dvec3 centroid(0.0);
            for (uint32_t id : ids) {
                centroid += points[id];
            }
            const uint32_t apex = addPoint(centroid / static_cast<double>(ids.size()));

            for (const auto& ring : faces) {
                const auto first = std::min_element(ring.begin(), ring.end());
                const size_t start = static_cast<size_t>(first - ring.begin());
                const size_t n = ring.size();
                for (size_t k = 1; k + 1 < n; ++k) {
                    addTet({apex, ring[start], ring[(start + k) % n], ring[(start + k + 1) % n]},
                           c);
                }
            }
        }
    }

    void addTet(const Tet& tet, uint32_t cell) {
        const auto t = static_cast<uint32_t>(tets.size());
        tets.push_back(tet);
        tetCells.push_back(cell);
        alive.push_back(true);
        uint32_t slot = 6 * t;
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = i + 1; j < 4; ++j, ++slot) {
                edgeNext.push_back(NO_SLOT);
                const auto [it, inserted] =
                    edgeLists.try_emplace(edgeKey(tet[i], tet[j]), EdgeList{slot, slot});
                if (!inserted) {
                    edgeNext[it->second.tail] = slot;
                    it->second.tail = slot;
                }
            }
        }
        pending.push_back(t);
    }

    uint32_t addPoint(const glm::dvec3& p) {
        points.push_back(p);
        values.push_back(std::numeric_limits<double>::quiet_NaN());
        return static_cast<uint32_t>(points.size() - 1);
    }

    uint32_t midpoint(uint32_t a, uint32_t b) {
        const auto [it, inserted] = midpoints.try_emplace(edgeKey(a, b), 0u);
        if (inserted) {
            it->second = addPoint(0.5 * (points[a] + points[b]));
        }
        return it->second;
    }

    double value(uint32_t id, uint32_t cell) { return marcher.value(id, cell); }

    std::array<glm::dvec3, 4> positions(const Tet& tet) const {
        return {points[tet[0]], points[tet[1]], points[tet[2]], points[tet[3]]};
    }

    // Ties broken by key, so "longest" is a strict order and bisection
    // propagation terminates.
    uint64_t longestEdgeKey(uint32_t t) const {
        const Tet& tet = tets[t];
        std::pair<double, uint64_t> best{-1.0, 0};
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = i + 1; j < 4; ++j) {
                best = std::max(best, std::pair<double, uint64_t>{
                                          glm::length(points[tet[j]] - points[tet[i]]),
                                          edgeKey(tet[i], tet[j])});
            }
        }
        return best.second;
    }

    // F minus its linear interpolant on a tet is -sum_{i<j} l_i l_j e_ij^T A e_ij
    // (l barycentric), bounded by (3/8) |A| h^2. Vertex values all beyond that
    // on one side means no zero in the closed tet.
    bool missesSkin(const Tet& tet, uint32_t cell) {
        const double h = longestEdge(positions(tet));
        const double bound = 0.375 * quadrics[cell].formNorm * h * h;
        double lo = std::numeric_limits<double>::infinity();
        double hi = -lo;
        for (uint32_t id : tet) {
            const double f = value(id, cell);
            lo = std::min(lo, f);
            hi = std::max(hi, f);
        }
        const double slack = 1e-9 * (bound + std::max(std::abs(lo), std::abs(hi)));
        return lo > bound + slack || hi < -bound - slack;
    }

    bool needsRefinement(uint32_t t) {
        const Tet& tet = tets[t];
        const uint32_t cell = tetCells[t];
        return longestEdge(positions(tet)) > targets[cell] && !missesSkin(tet, cell);
    }

    // Rivara: an edge is split in every tet that holds it, so the mesh stays
    // conforming across cells. A tet whose longest edge is another one has
    // that split first.
    void bisect(uint64_t key) {
        while (true) {
            const auto it = edgeLists.find(key);
            if (it == edgeLists.end()) {
                return;
            }
            uint32_t slot = it->second.head;
            while (slot != NO_SLOT && !alive[slot / 6]) {
                slot = edgeNext[slot];
            }
            if (slot == NO_SLOT) {
                edgeLists.erase(it);
                return;
            }
            const uint32_t t = slot / 6;
            const uint64_t longest = longestEdgeKey(t);
            if (longest != key) {
                bisect(longest);
                continue;
            }
            split(t, key);
        }
    }

    void split(uint32_t t, uint64_t key) {
        const auto a = static_cast<uint32_t>(key >> 32);
        const auto b = static_cast<uint32_t>(key & 0xFFFFFFFFu);
        const uint32_t m = midpoint(a, b);
        alive[t] = false;
        Tet first = tets[t];
        Tet second = tets[t];
        std::replace(first.begin(), first.end(), b, m);
        std::replace(second.begin(), second.end(), a, m);
        const uint32_t cell = tetCells[t];
        addTet(first, cell);
        addTet(second, cell);
    }

};

} // namespace

MarchingTetsResult marchTets(const MixedComplex& mixed, const MixedComplexTopology& topology,
                             const std::vector<SkinQuadric>& quadrics, double k,
                             MeshingControl* control) {
    Mesher mesher(mixed, topology, quadrics);
    return mesher.run(topology, k, control);
}

} // namespace skin::geometry
