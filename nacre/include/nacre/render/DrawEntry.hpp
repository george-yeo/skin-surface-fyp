#pragma once

#include <cstddef>

#include <glm/glm.hpp>
#include <nacre/assets/Handles.hpp>

namespace nacre::render {

struct DrawEntry {
    struct Mesh {
        assets::MeshHandle meshHandle;
        assets::MaterialHandle materialHandle;
        glm::mat4 transform;
        glm::vec4 color;
    };

    // GPU-visible weighted point: matches the std430 layout of the Balls SSBO
    // in visualisation_ball.vert/.frag exactly (vec3 + float packs to 16
    // bytes both sides).
    struct Ball {
        glm::vec3 position;
        float radius;
    };

    // GPU-visible line segment with thickness and colour, drawn as a
    // billboarded capsule in visualisation_line.vert/.frag. std430 aligns
    // each vec3 to 16 bytes, so `_pad0` makes the C++ layout match the GLSL
    // one exactly: start@0, end@16, thickness@28, color@32, size 48. Alpha is
    // carried but unused (the line pipeline has no blending).
    struct Line {
        glm::vec3 start;
        float _pad0;
        glm::vec3 end;
        float thickness;
        glm::vec4 color;
    };
};

// Catch C++/GLSL layout drift at compile time instead of as garbled SSBO reads.
static_assert(sizeof(DrawEntry::Ball) == 16);
static_assert(offsetof(DrawEntry::Ball, radius) == 12);
static_assert(sizeof(DrawEntry::Line) == 48);
static_assert(offsetof(DrawEntry::Line, end) == 16);
static_assert(offsetof(DrawEntry::Line, thickness) == 28);
static_assert(offsetof(DrawEntry::Line, color) == 32);

} // namespace nacre::render
