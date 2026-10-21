#include "skin/geometry/TetMarcher.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace skin::geometry {

namespace {

constexpr double ROOT_TOLERANCE = 1e-9;
constexpr int BISECTION_STEPS = 64;

uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | b;
}

} // namespace

double crossingParameter(const SkinQuadric& q, const glm::dvec3& a, const glm::dvec3& b,
                         double fa, double fb) {
    const glm::dvec3 e = b - a;
    const glm::dvec3 d0 = a - q.centre;
    const double alpha = glm::dot(e, q.form * e);
    const double beta = glm::dot(d0, q.form * e);
    const double gamma = q.value(a);

    const auto inRange = [](double t) { return t >= -ROOT_TOLERANCE && t <= 1.0 + ROOT_TOLERANCE; };
    const double scale = std::abs(alpha) + std::abs(beta) + std::abs(gamma);
    if (std::abs(alpha) <= 1e-14 * scale) {
        if (beta != 0.0) {
            const double t = -gamma / (2.0 * beta);
            if (inRange(t)) {
                return std::clamp(t, 0.0, 1.0);
            }
        }
    } else {
        const double discriminant = std::max(beta * beta - alpha * gamma, 0.0);
        const double q0 = -(beta + std::copysign(std::sqrt(discriminant), beta));
        std::array<double, 2> roots = {q0 / alpha, q0 != 0.0 ? gamma / q0 : q0 / alpha};
        // With a sign change there is exactly one root in [0, 1]; prefer the
        // one nearer the middle if rounding admits both.
        std::sort(roots.begin(), roots.end(),
                  [](double x, double y) { return std::abs(x - 0.5) < std::abs(y - 0.5); });
        for (const double t : roots) {
            if (inRange(t)) {
                return std::clamp(t, 0.0, 1.0);
            }
        }
    }

    // Degenerate or rounding-inconsistent: bisect on this cell's F, else
    // interpolate the cached values.
    double lo = 0.0;
    double hi = 1.0;
    double flo = gamma;
    if ((flo < 0.0) == (q.value(b) < 0.0)) {
        return std::clamp(fa / (fa - fb), 0.0, 1.0);
    }
    for (int step = 0; step < BISECTION_STEPS; ++step) {
        const double mid = 0.5 * (lo + hi);
        const double fmid = q.value(a + mid * e);
        if ((fmid < 0.0) == (flo < 0.0)) {
            lo = mid;
            flo = fmid;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

TetMarcher::TetMarcher(const std::vector<glm::dvec3>& points, std::vector<double>& values,
                       const std::vector<SkinQuadric>& quadrics,
                       std::vector<uint8_t> cellDimensions)
    : points(points), values(values), quadrics(quadrics),
      cellDimensions(std::move(cellDimensions)) {}

double TetMarcher::value(uint32_t point, uint32_t cell) {
    if (std::isnan(values[point])) {
        values[point] = quadrics[cell].value(points[point]);
    }
    return values[point];
}

uint32_t TetMarcher::crossing(uint32_t a, uint32_t b, uint32_t cell) {
    if (a > b) {
        std::swap(a, b);
    }
    const auto [it, inserted] = crossings.try_emplace(edgeKey(a, b), 0u);
    if (!inserted) {
        return it->second;
    }
    const SkinQuadric& q = quadrics[cell];
    const double t = crossingParameter(q, points[a], points[b], values[a], values[b]);
    const glm::dvec3 x = points[a] + t * (points[b] - points[a]);
    const glm::dvec3 g = q.gradient(x);
    const double length = glm::length(g);

    it->second = static_cast<uint32_t>(mesh.positions.size());
    mesh.positions.push_back(x);
    mesh.normals.push_back(length > 0.0 ? g / length : glm::dvec3(0.0));
    return it->second;
}

void TetMarcher::addTriangle(uint32_t a, uint32_t b, uint32_t c, const glm::dvec3& outward,
                             uint32_t cell) {
    const glm::dvec3 n =
        glm::cross(mesh.positions[b] - mesh.positions[a], mesh.positions[c] - mesh.positions[a]);
    if (glm::dot(n, outward) < 0.0) {
        std::swap(b, c);
    }
    mesh.triangles.push_back({a, b, c});
    mesh.triangleCell.push_back(cell);
    mesh.triangleDimension.push_back(cellDimensions[cell]);
}

void TetMarcher::march(const std::array<uint32_t, 4>& tet, uint32_t cell) {
    std::array<uint32_t, 4> inside{};
    std::array<uint32_t, 4> outside{};
    size_t insideCount = 0;
    size_t outsideCount = 0;
    glm::dvec3 insideMean(0.0);
    glm::dvec3 outsideMean(0.0);
    for (uint32_t id : tet) {
        if (value(id, cell) < 0.0) {
            inside[insideCount++] = id;
            insideMean += points[id];
        } else {
            outside[outsideCount++] = id;
            outsideMean += points[id];
        }
    }
    if (insideCount == 0 || outsideCount == 0) {
        return;
    }
    const glm::dvec3 outward = outsideMean / static_cast<double>(outsideCount) -
                               insideMean / static_cast<double>(insideCount);

    if (insideCount == 1 || outsideCount == 1) {
        const bool loneInside = insideCount == 1;
        const uint32_t lone = loneInside ? inside[0] : outside[0];
        const auto& others = loneInside ? outside : inside;
        addTriangle(crossing(lone, others[0], cell), crossing(lone, others[1], cell),
                    crossing(lone, others[2], cell), outward, cell);
        return;
    }

    // Two and two: the crossings form a quad i0o0, i0o1, i1o1, i1o0.
    const uint32_t a = crossing(inside[0], outside[0], cell);
    const uint32_t b = crossing(inside[0], outside[1], cell);
    const uint32_t c = crossing(inside[1], outside[1], cell);
    const uint32_t d = crossing(inside[1], outside[0], cell);
    addTriangle(a, b, c, outward, cell);
    addTriangle(a, c, d, outward, cell);
}

SkinMesh TetMarcher::finish() {
    fillMissingNormals(mesh);
    return std::move(mesh);
}

} // namespace skin::geometry
