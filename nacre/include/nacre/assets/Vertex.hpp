#pragma once

#include <format>
#include <glm/glm.hpp>

namespace nacre::assets {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal = {0.0f, 1.0f, 0.0f};
    glm::vec3 color = {1.0f, 1.0f, 1.0f};

    bool operator==(const Vertex& other) const noexcept {
        return position == other.position && normal == other.normal && color == other.color;
    }
};

} // namespace nacre::assets

template <>
struct std::formatter<nacre::assets::Vertex> : std::formatter<std::string> {
    auto format(const nacre::assets::Vertex& v, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("({}, {}, {}), normal=({}, {}, {}), color=({}, {}, {})", v.position.x,
                        v.position.y, v.position.z, v.normal.x, v.normal.y, v.normal.z,
                        v.color.r, v.color.g, v.color.b),
            ctx);
    }
};
