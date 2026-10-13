#pragma once

#include <nacre/assets/Vertex.hpp>

#include <cstdint>
#include <vector>

namespace nacre::assets {

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool hasIndices() const { return !indices.empty(); }

    MeshData() = default;
    MeshData(std::vector<Vertex> vertices) : vertices(std::move(vertices)) {}
    MeshData(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
        : vertices(std::move(vertices)), indices(std::move(indices)) {}
};

} // namespace nacre::assets
