#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include <nacre/shaders/fwd.hpp>
#include <nacre/vk/Buffer.hpp>
#include <nacre/vk/DescriptorSet.hpp>
#include <nacre/vk/DescriptorSetLayout.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::render {

// Per-frame data for the mesh pass: camera UBO (set 0) and object SSBO (set 2).
class FrameResources {
    public:
    FrameResources(const vk::RenderContext& renderContext, uint32_t objectCapacity);
    ~FrameResources();

    FrameResources(const FrameResources&) = delete;
    FrameResources& operator=(const FrameResources&) = delete;

    // FrameResources(FrameResources&&) noexcept;
    // FrameResources& operator=(FrameResources&&) noexcept;

    void uploadCamera(const shaders::CameraData& camera);
    // Writes at most getMaxObjects() entries.
    void uploadObjects(std::span<const shaders::ObjectData> objects);

    const vk::DescriptorSet& getSceneSet() const { return *sceneSet; }
    const vk::DescriptorSet& getObjectSet() const { return *objectSet; }
    uint32_t getMaxObjects() const { return maxObjects; }

    private:
    vk::Device& device;
    uint32_t maxObjects;

    std::unique_ptr<vk::DescriptorSet> sceneSet;
    std::unique_ptr<vk::Buffer> sceneUbo;
    std::unique_ptr<vk::DescriptorSet> objectSet;
    std::unique_ptr<vk::Buffer> objectSsbo;
};

} // namespace nacre::render
