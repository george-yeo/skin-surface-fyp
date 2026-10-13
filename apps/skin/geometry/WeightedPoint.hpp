#pragma once

#include <glm/glm.hpp>

namespace skin::geometry {

struct WeightedPoint {
    glm::vec3 position;
    double weight;

    WeightedPoint(glm::vec3 pos, double w) : position(pos), weight(w) {}
    WeightedPoint(glm::vec2 pos, double w) : position(glm::vec3(pos, 0.0)), weight(w) {}
};

} // namespace skin::geometry
