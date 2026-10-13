#pragma once

#include <memory>
#include <span>
#include <vector>

#include <nacre/vk/Buffer.hpp>
#include <nacre/assets/Vertex.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::assets {

class Mesh {
    public:
    explicit Mesh(std::span<const Vertex> verts);
    explicit Mesh(std::span<const Vertex> verts, std::span<const uint32_t> indices);

    vk::Buffer& getBuffer() const { return *buffer; }
    vk::Buffer& getIndexBuffer() const { return *indexBuffer; }

    uint32_t getVertexCount() const { return static_cast<uint32_t>(vertices.size()); }
    uint32_t getIndexCount() const { return static_cast<uint32_t>(indices.size()); }

    void createGpuResources(vk::Device& device);
    void destroyGpuResources();

    bool isGpuReady() const { return hasLoadedBuffer; }
    bool hasIndices() const { return !indices.empty(); }

    void incrementReferenceCount() { referenceCount++; }
    void decrementReferenceCount() {
        if (referenceCount == 0) {
            core::Logger("Mesh").fatal("decrementReferenceCount: reference count is already 0");
        }
        referenceCount--;
    }
    bool isReferenced() const { return referenceCount > 0; }

    private:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unique_ptr<vk::Buffer> buffer;
    std::unique_ptr<vk::Buffer> indexBuffer;
    uint32_t referenceCount = 0;
    bool hasLoadedBuffer = false;
};

} // namespace nacre::assets
