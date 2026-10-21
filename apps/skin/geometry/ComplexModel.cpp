#include "skin/geometry/ComplexModel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "skin/geometry/WeightedPointParser.hpp"

namespace skin::geometry {

namespace {

// A regular tetrahedron at distance D from the balls' centre, weight -D^2.
// For a mixed cell mu_X with a ghost g in X, w_X = |g - z_X|^2 + D^2 >= D^2,
// and |u|^2 / (1 - s) <= (1 - s) max_{i in X} |z_i - z_X|^2
// = (1 - s) max_{i in X} (w_X + w_i), so F >= s D^2 - (1 - s) max w_i > 0:
// no skin. D >= 20 r_max covers s > 1/401.
std::vector<WeightedPoint> ghostPoints(const std::vector<WeightedPoint>& points) {
    glm::dvec3 lo(std::numeric_limits<double>::infinity());
    glm::dvec3 hi(-std::numeric_limits<double>::infinity());
    for (const WeightedPoint& point : points) {
        const double radius = std::sqrt(std::max(point.weight, 0.0));
        lo = glm::min(lo, glm::dvec3(point.position) - radius);
        hi = glm::max(hi, glm::dvec3(point.position) + radius);
    }
    const glm::dvec3 centre = 0.5 * (lo + hi);
    const double distance = 10.0 * glm::length(hi - lo) + 1.0;

    const std::array<glm::dvec3, 4> directions = {
        glm::dvec3(1, 1, 1), glm::dvec3(1, -1, -1), glm::dvec3(-1, 1, -1), glm::dvec3(-1, -1, 1)};
    std::vector<WeightedPoint> ghosts;
    for (const glm::dvec3& direction : directions) {
        const glm::dvec3 position = centre + distance * glm::normalize(direction);
        ghosts.emplace_back(glm::vec3(position), -distance * distance);
    }
    return ghosts;
}

} // namespace

void ComplexModel::load(const std::string& path) {
    inputPoints = WeightedPointParser::parseFile(path);
}

void ComplexModel::addPoint(const WeightedPoint& point) {
    inputPoints.push_back(point);
}

void ComplexModel::rebuild(double skinPadding, double displayPadding, double s) {
    ghosts = 0;
    powerComplex.build(inputPoints);
    if (!inputPoints.empty() && powerComplex.dimension() < 3) {
        std::vector<WeightedPoint> augmented = inputPoints;
        const std::vector<WeightedPoint> extra = ghostPoints(inputPoints);
        augmented.insert(augmented.end(), extra.begin(), extra.end());
        ghosts = static_cast<uint32_t>(extra.size());
        powerComplex.build(augmented);
    }
    rebuildClipping(skinPadding, displayPadding, s);
}

void ComplexModel::rebuildClipping(double skinPadding, double displayPadding, double s) {
    const Aabb skinBox =
        skinBounds(powerComplex, skinPadding, static_cast<uint32_t>(inputPoints.size()));
    skinClip = buildBoundedVoronoi(powerComplex, skinBox);
    displayClip =
        buildBoundedVoronoi(powerComplex, displayBounds(powerComplex, skinBox, displayPadding));
    rebuildMixed(s);
}

void ComplexModel::rebuildMixed(double s) {
    mixed = buildMixedComplex(powerComplex, skinClip.box, s);
    topology = weldMixedComplex(mixed);
    quadrics = buildSkinQuadrics(powerComplex, mixed, s);
    logWallAgreement(mixed, topology, quadrics, skinClip.box);
    comparison.reset();
}

SkinInputs ComplexModel::skinInputs() const {
    return {.points = inputPoints, .mixed = mixed, .topology = topology, .quadrics = quadrics};
}

} // namespace skin::geometry
