#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include <unordered_map>

#include <nacre/assets/MeshObjLoader.hpp>

namespace nacre::assets {

struct VertexHash {
    std::size_t operator()(const Vertex& v) const noexcept {
        std::size_t h1 = std::hash<float>()(v.position.x) ^
                         (std::hash<float>()(v.position.y) << 1) ^
                         (std::hash<float>()(v.position.z) << 2);
        std::size_t h2 = std::hash<float>()(v.normal.x) ^ (std::hash<float>()(v.normal.y) << 1) ^
                         (std::hash<float>()(v.normal.z) << 2);
        return h1 ^ (h2 << 1);
    }
};

MeshData MeshObjLoader::loadFromFile(const std::string& path) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, config)) {
        throw std::runtime_error("Failed to parse OBJ file: " + path + "\n" + reader.Error());
    }

    if (!reader.Warning().empty()) {
        std::fprintf(stderr, "tinyobjloader warning (%s): %s\n", path.c_str(),
                     reader.Warning().c_str());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    if (shapes.empty()) {
        throw std::runtime_error("No shapes found in OBJ file: " + path);
    }

    MeshData result{};
    std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const tinyobj::index_t& idx : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = glm::vec3(attrib.vertices[3 * idx.vertex_index + 0],
                                        attrib.vertices[3 * idx.vertex_index + 1],
                                        attrib.vertices[3 * idx.vertex_index + 2]);

            if (idx.normal_index >= 0 && !attrib.normals.empty()) {
                vertex.normal = glm::vec3(attrib.normals[3 * idx.normal_index + 0],
                                          attrib.normals[3 * idx.normal_index + 1],
                                          attrib.normals[3 * idx.normal_index + 2]);
            } // todo: handle missing normals

            auto it = uniqueVertices.find(vertex);
            if (it == uniqueVertices.end()) {
                uint32_t newIndex = static_cast<uint32_t>(result.vertices.size());
                uniqueVertices.emplace(vertex, newIndex);
                result.vertices.push_back(vertex);
                result.indices.push_back(newIndex);
            } else {
                result.indices.push_back(it->second);
            }
        }
    }

    return result;
}

} // namespace nacre::assets
