#pragma once

#include <glm/glm.hpp>

namespace nacre::scene {

// Self-contained world-space line segment (no TransformComponent: a segment
// doesn't have a single "position" to transform). Mirrors DrawEntry::Line.
struct LineComponent {
    glm::vec3 start;
    glm::vec3 end;
    float thickness;
};

} // namespace nacre::scene
