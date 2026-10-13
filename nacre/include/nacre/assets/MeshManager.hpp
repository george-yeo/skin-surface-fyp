#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include <nacre/assets/Vertex.hpp>
#include <nacre/core/ActionQueue.hpp>
#include <nacre/core/Logger.hpp>
#include <nacre/assets/Mesh.hpp>
#include <nacre/assets/Handles.hpp>
#include <nacre/assets/MeshTypes.hpp>
#include <nacre/assets/fwd.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::assets {

class MeshManager {
    public:
    explicit MeshManager(vk::Device& device);
    ~MeshManager();

    void addMesh(const std::string& name, const MeshData& meshData);

    std::optional<MeshHandle> getHandle(
        const std::string& name); // todo: make handle count and cloning of handle
    void releaseHandle(const MeshHandle& handle);

    Mesh& getMesh(const MeshHandle& handle);
    bool hasMesh(const std::string& name) const;

    bool canRender(const MeshHandle& handle) const;
    void createGpuResources(const MeshHandle& handle);
    void createGpuResourcesAll();
    void destroyGpuResources(const MeshHandle& handle);
    void destroyAllGpuResources();
    // Vertex buffer, plus the index buffer if the mesh has one.
    void bindMesh(const MeshHandle& handle, vk::CommandBuffer& commandBuffer);
    void drawMesh(const MeshHandle& handle, vk::CommandBuffer& commandBuffer);

    void update();

    private:
    core::Logger logger{"MeshManager"};

    vk::Device& device;
    uint32_t nextId = 1;

    std::unordered_map<std::string, MeshHandle> nameToHandleRegistry;
    std::unordered_map<uint32_t, std::unique_ptr<Mesh>> loadedMeshes;

    std::queue<uint32_t> deleteQueue;
};

} // namespace nacre::assets
