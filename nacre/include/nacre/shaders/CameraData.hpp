#pragma once

#include <cstddef>

#include <glm/glm.hpp>

namespace nacre::shaders {

// `Camera` at set 0, binding 0 in test_shader and visualisation_*.
struct CameraData {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
    alignas(16) glm::vec4 position;
};

static_assert(sizeof(CameraData) == 144);
static_assert(offsetof(CameraData, projection) == 64);
static_assert(offsetof(CameraData, position) == 128);

} // namespace nacre::shaders
