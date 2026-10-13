#include <algorithm>

#include <nacre/vk/Buffer.hpp>
#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/assets/MeshManager.hpp>

#include <vulkan/vulkan.h>

namespace nacre::assets {

MeshManager::MeshManager(vk::Device& dev) : device(dev) {}

void MeshManager::addMesh(const std::string& name, const MeshData& meshData) {
    if (nameToHandleRegistry.find(name) != nameToHandleRegistry.end()) {
        logger.fatal("addMesh: mesh '{}' already exists", name);
    }

    MeshHandle handle{nextId++};
    if (meshData.hasIndices()) {
        loadedMeshes.emplace(handle.id,
                             std::make_unique<Mesh>(meshData.vertices, meshData.indices));
    } else {
        loadedMeshes.emplace(handle.id, std::make_unique<Mesh>(meshData.vertices));
    }

    createGpuResources(handle); // temp

    nameToHandleRegistry.insert({name, handle});

    logger.print("Added mesh '{}' with handle ID {}", name, handle.id);
}

std::optional<MeshHandle> MeshManager::getHandle(const std::string& name) {
    auto it = nameToHandleRegistry.find(name);

    if (it == nameToHandleRegistry.end()) {
        return std::nullopt;
    }

    return it->second;
}

void MeshManager::releaseHandle(const MeshHandle& handle) {
    auto it = std::find_if(nameToHandleRegistry.begin(), nameToHandleRegistry.end(),
                           [&handle](const auto& pair) { return pair.second.id == handle.id; });

    if (it == nameToHandleRegistry.end()) {
        logger.fatal("releaseHandle: mesh handle {} is not registered (double release?)", handle.id);
    }
    logger.print("Releasing mesh '{}' with handle ID {}", it->first, handle.id);
    nameToHandleRegistry.erase(it);
    deleteQueue.push(handle.id);
}

Mesh& MeshManager::getMesh(const MeshHandle& handle) {
    auto it = loadedMeshes.find(handle.id);
    if (it == loadedMeshes.end() || it->second == nullptr) {
        logger.fatal("getMesh: mesh handle {} not found", handle.id);
    }

    return *it->second;
}

bool MeshManager::hasMesh(const std::string& name) const {
    return nameToHandleRegistry.find(name) != nameToHandleRegistry.end();
}

bool MeshManager::canRender(const MeshHandle& handle) const {
    auto it = loadedMeshes.find(handle.id);
    if (it == loadedMeshes.end() || !it->second) {
        return false;
    }

    return it->second->isGpuReady();
}

void MeshManager::createGpuResources(const MeshHandle& handle) {
    auto it = loadedMeshes.find(handle.id);
    if (it == loadedMeshes.end() || !it->second) {
        logger.fatal("createGpuResources: mesh {} not found", handle.id);
    }

    if (!it->second->isGpuReady()) {
        it->second->createGpuResources(device);

        logger.print("GPU buffer created for mesh {}", handle.id);
    }
}

void MeshManager::createGpuResourcesAll() {
    for (const auto& [id, mesh] : loadedMeshes) {
        createGpuResources(MeshHandle{id});
    }
}

void MeshManager::destroyGpuResources(const MeshHandle& handle) {
    auto it = loadedMeshes.find(handle.id);
    if (it == loadedMeshes.end() || !it->second) {
        logger.fatal("destroyGpuResources: mesh {} not found", handle.id);
    }

    if (it->second->isGpuReady()) {
        it->second->destroyGpuResources();

        logger.print("GPU resources destroyed for mesh {}", handle.id);
    }
}

void MeshManager::destroyAllGpuResources() {
    for (auto& [id, mesh] : loadedMeshes) {
        mesh->destroyGpuResources();
    }
    logger.print("All GPU resources destroyed for MeshManager meshes");
}

MeshManager::~MeshManager() {
    destroyAllGpuResources();
}

void MeshManager::bindMesh(const MeshHandle& handle, vk::CommandBuffer& commandBuffer) {
    auto it = loadedMeshes.find(handle.id);
    if (it == loadedMeshes.end() || !it->second) {
        logger.fatal("bindMesh: mesh {} not found", handle.id);
    }

    VkBuffer buffer = it->second->getBuffer().get();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer.get(), 0, 1, &buffer, &offset);

    if (it->second->hasIndices()) {
        VkBuffer indexBuffer = it->second->getIndexBuffer().get();
        vkCmdBindIndexBuffer(commandBuffer.get(), indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
}

void MeshManager::drawMesh(const MeshHandle& handle, vk::CommandBuffer& commandBuffer) {
    auto it = loadedMeshes.find(handle.id);
    if (it == loadedMeshes.end() || !it->second) {
        logger.fatal("drawMesh: mesh {} not found", handle.id);
    }

    if (it->second->hasIndices()) {
        vkCmdDrawIndexed(commandBuffer.get(), it->second->getIndexCount(), 1, 0, 0, 0);
    } else {
        vkCmdDraw(commandBuffer.get(), it->second->getVertexCount(), 1, 0, 0);
    }
}

void MeshManager::update() {
    // One pass over the queue: meshes still referenced by an in-flight frame go
    // back to the end instead of blocking everything queued behind them.
    for (size_t pending = deleteQueue.size(); pending > 0; --pending) {
        uint32_t id = deleteQueue.front();
        deleteQueue.pop();

        auto it = loadedMeshes.find(id);
        if (it == loadedMeshes.end()) {
            continue;
        }

        if (it->second->isReferenced()) {
            deleteQueue.push(id);
            continue;
        }

        if (it->second->isGpuReady()) {
            it->second->destroyGpuResources();
            logger.print("GPU resources destroyed for mesh {}", id);
        }
        loadedMeshes.erase(it);
    }
}

} // namespace nacre::assets
