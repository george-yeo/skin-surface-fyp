#pragma once

#include <cstddef>

#include <glm/glm.hpp>

namespace nacre::shaders {

// One element of the `Object` SSBO at set 2, binding 0 in test_shader.
struct ObjectData {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 color;
};

static_assert(sizeof(ObjectData) == 80);
static_assert(offsetof(ObjectData, color) == 64);

} // namespace nacre::shaders
