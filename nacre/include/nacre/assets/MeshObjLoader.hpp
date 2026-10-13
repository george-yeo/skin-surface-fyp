#pragma once

#include <nacre/assets/Mesh.hpp>
#include <nacre/assets/MeshTypes.hpp>

namespace nacre::assets {

class MeshObjLoader {
    public:
    MeshObjLoader() = default;
    ~MeshObjLoader() = default;

    MeshObjLoader(const MeshObjLoader&) = delete;
    MeshObjLoader& operator=(const MeshObjLoader&) = delete;

    MeshObjLoader(MeshObjLoader&&) noexcept = default;
    MeshObjLoader& operator=(MeshObjLoader&&) noexcept = default;

    static MeshData loadFromFile(const std::string& path);
};

} // namespace nacre::assets
