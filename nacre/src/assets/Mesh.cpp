#include <nacre/assets/Mesh.hpp>

namespace nacre::assets {

Mesh::Mesh(std::span<const Vertex> verts) : vertices(verts.begin(), verts.end()) {}
Mesh::Mesh(std::span<const Vertex> verts, std::span<const uint32_t> idx)
    : vertices(verts.begin(), verts.end()), indices(idx.begin(), idx.end()) {}

void Mesh::createGpuResources(vk::Device& device) {
    if (isGpuReady()) {
        core::Logger("Mesh").fatal("createGpuResources: GPU buffer already exists");
    }

    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
    buffer = std::make_unique<vk::Buffer>(
        device, vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    buffer->write(vertices.data(), vertexBufferSize);

    if (hasIndices()) {
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
        indexBuffer = std::make_unique<vk::Buffer>(
            device, indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        indexBuffer->write(indices.data(), indexBufferSize);
    }

    hasLoadedBuffer = true;
}

void Mesh::destroyGpuResources() {
    buffer.reset();
    indexBuffer.reset();
    hasLoadedBuffer = false;
}

} // namespace nacre::assets
