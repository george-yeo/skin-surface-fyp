#include "skin/geometry/KvRefinement.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "skin/geometry/KruithofVegter.hpp"
#include "skin/geometry/SkinBuild.hpp"
#include "skin/geometry/SkinMesh.hpp"
#include "skin/geometry/TetMarcher.hpp"

namespace skin::geometry {

namespace {

// Triangles between stop checks and progress updates.
constexpr size_t CHECK_INTERVAL = 4096;

uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | b;
}

struct Refiner {
    const KvTetComplex& complex;
    std::span<const double> values;
    SkinMesh& mesh;
    size_t unmovedPoints = 0;
    double worstResidual = 0.0;

    // Moves p, a point of the tet, along the tet's transversal segment through
    // it to the skin. With barycentric coordinates lambda of p, a and b are the
    // lambda-weighted means of the inside and outside vertices, so p = (sum of
    // inside lambda) a + (sum of outside lambda) b lies on ab: the line from the
    // lone vertex (1|3) or the line meeting both edges (2|2). On a face the
    // off-face lambda is 0, so neighbouring tets agree there. F is monotone on
    // ab [KV07, Lemma 19], so its root is unique.
    uint32_t addPoint(const glm::dvec3& p, uint32_t tet) {
        const std::array<uint32_t, 4>& ids = complex.tets[tet];
        const SkinQuadric& q = complex.quadrics[complex.tetCells[tet]];
        const glm::dvec3 v0 = complex.points[ids[0]];
        const glm::dmat3 edges(complex.points[ids[1]] - v0, complex.points[ids[2]] - v0,
                               complex.points[ids[3]] - v0);
        const glm::dvec3 l = glm::inverse(edges) * (p - v0);
        const std::array<double, 4> lambda = {1.0 - l.x - l.y - l.z, l.x, l.y, l.z};

        glm::dvec3 a(0.0);
        glm::dvec3 b(0.0);
        double insideWeight = 0.0;
        double outsideWeight = 0.0;
        for (size_t k = 0; k < 4; ++k) {
            const double weight = std::max(lambda[k], 0.0);
            if (values[ids[k]] < 0.0) {
                a += weight * complex.points[ids[k]];
                insideWeight += weight;
            } else {
                b += weight * complex.points[ids[k]];
                outsideWeight += weight;
            }
        }

        glm::dvec3 x = p;
        bool moved = false;
        if (insideWeight > 0.0 && outsideWeight > 0.0) {
            a /= insideWeight;
            b /= outsideWeight;
            const double fa = q.value(a);
            const double fb = q.value(b);
            if (fa < 0.0 && fb >= 0.0) {
                x = a + crossingParameter(q, a, b, fa, fb) * (b - a);
                moved = true;
            }
        }
        if (!moved) {
            ++unmovedPoints;
        }

        const glm::dvec3 g = q.gradient(x);
        const double length = glm::length(g);
        if (length > 0.0) {
            worstResidual = std::max(worstResidual, std::abs(q.value(x)) / length);
        }
        mesh.positions.push_back(x);
        mesh.normals.push_back(length > 0.0 ? g / length : glm::dvec3(0.0));
        return static_cast<uint32_t>(mesh.positions.size() - 1);
    }

    // One 1-to-9 split of every triangle; false if stopped.
    bool round(std::vector<uint32_t>& triangleTets, MeshingControl* control, int index,
               int rounds) {
        const size_t count = mesh.triangles.size();
        std::vector<std::array<uint32_t, 3>> triangles;
        std::vector<uint32_t> cells;
        std::vector<uint8_t> dimensions;
        std::vector<uint32_t> tets;
        triangles.reserve(9 * count);
        cells.reserve(9 * count);
        dimensions.reserve(9 * count);
        tets.reserve(9 * count);

        // Edge (lo, hi) -> its points at 1/3 and 2/3 from lo.
        std::unordered_map<uint64_t, std::array<uint32_t, 2>> edgePoints;
        const auto trisect = [&](uint32_t from, uint32_t to,
                                 uint32_t tet) -> std::array<uint32_t, 2> {
            const uint32_t lo = std::min(from, to);
            const uint32_t hi = std::max(from, to);
            const auto [it, inserted] = edgePoints.try_emplace(edgeKey(lo, hi));
            if (inserted) {
                const glm::dvec3 pl = mesh.positions[lo];
                const glm::dvec3 ph = mesh.positions[hi];
                const uint32_t nearLo = addPoint((2.0 * pl + ph) / 3.0, tet);
                const uint32_t nearHi = addPoint((pl + 2.0 * ph) / 3.0, tet);
                it->second = {nearLo, nearHi};
            }
            const std::array<uint32_t, 2> points = it->second;
            return from == lo ? points : std::array<uint32_t, 2>{points[1], points[0]};
        };

        for (size_t t = 0; t < count; ++t) {
            if (control != nullptr && t % CHECK_INTERVAL == 0) {
                if (control->stopRequested()) {
                    return false;
                }
                control->fraction = (static_cast<float>(index) +
                                     static_cast<float>(t) / static_cast<float>(count)) /
                                    static_cast<float>(rounds);
            }
            const auto [a, b, c] = mesh.triangles[t];
            const uint32_t tet = triangleTets[t];
            const std::array<uint32_t, 2> ab = trisect(a, b, tet);
            const std::array<uint32_t, 2> bc = trisect(b, c, tet);
            const std::array<uint32_t, 2> ca = trisect(c, a, tet);
            const uint32_t centre =
                addPoint((mesh.positions[a] + mesh.positions[b] + mesh.positions[c]) / 3.0, tet);

            // grid[i][j] = a + i/3 (b - a) + j/3 (c - a); both triangle kinds
            // below keep the orientation of abc.
            std::array<std::array<uint32_t, 4>, 4> grid{};
            grid[0][0] = a;
            grid[3][0] = b;
            grid[0][3] = c;
            grid[1][0] = ab[0];
            grid[2][0] = ab[1];
            grid[2][1] = bc[0];
            grid[1][2] = bc[1];
            grid[0][2] = ca[0];
            grid[0][1] = ca[1];
            grid[1][1] = centre;
            const auto emit = [&](uint32_t p, uint32_t q, uint32_t r) {
                triangles.push_back({p, q, r});
                cells.push_back(mesh.triangleCell[t]);
                dimensions.push_back(mesh.triangleDimension[t]);
                tets.push_back(tet);
            };
            for (size_t i = 0; i <= 2; ++i) {
                for (size_t j = 0; i + j <= 2; ++j) {
                    emit(grid[i][j], grid[i + 1][j], grid[i][j + 1]);
                }
            }
            for (size_t i = 0; i <= 1; ++i) {
                for (size_t j = 0; i + j <= 1; ++j) {
                    emit(grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]);
                }
            }
        }

        mesh.triangles = std::move(triangles);
        mesh.triangleCell = std::move(cells);
        mesh.triangleDimension = std::move(dimensions);
        triangleTets = std::move(tets);
        return true;
    }
};

} // namespace

KvRefinementResult refineKvMesh(const KvTetComplex& complex, std::span<const double> values,
                                int rounds, SkinMesh& mesh, std::vector<uint32_t>& triangleTets,
                                MeshingControl* control) {
    KvRefinementResult result;
    if (rounds <= 0) {
        return result;
    }
    if (control != nullptr) {
        control->phase = MeshingPhase::Refining;
        control->fraction = 0.0f;
    }
    Refiner refiner{complex, values, mesh};
    for (int r = 0; r < rounds; ++r) {
        if (!refiner.round(triangleTets, control, r, rounds)) {
            result.cancelled = true;
            return result;
        }
    }
    fillMissingNormals(mesh);
    result.unmovedPoints = refiner.unmovedPoints;
    result.worstResidual = refiner.worstResidual;
    return result;
}

size_t countIsotopyTestFailures(const KvTetComplex& complex, std::span<const double> values,
                                const SkinMesh& mesh, std::span<const uint32_t> triangleTets) {
    size_t failures = 0;
    for (size_t t = 0; t < mesh.triangles.size(); ++t) {
        const auto [a, b, c] = mesh.triangles[t];
        const glm::dvec3 p = mesh.positions[a];
        const glm::dvec3 n = glm::cross(mesh.positions[b] - p, mesh.positions[c] - p);
        for (const uint32_t id : complex.tets[triangleTets[t]]) {
            const double side = glm::dot(n, complex.points[id] - p);
            if (values[id] < 0.0 ? side >= 0.0 : side <= 0.0) {
                ++failures;
                break;
            }
        }
    }
    return failures;
}

} // namespace skin::geometry
