#pragma once

#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/Buffer.hpp>
#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/DescriptorSet.hpp>
#include <nacre/vk/DescriptorSetLayout.hpp>
#include <nacre/assets/MeshPass.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::assets {

class Material {
    public:
    explicit Material(const vk::RenderContext& renderContext);
    virtual ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    // Binds the pipeline for `pass` and the material's own set (set 1).
    // Returns the layout of the bound pipeline, for the caller's sets and push constants.
    virtual VkPipelineLayout bind(vk::CommandBuffer& commandBuffer, uint32_t frameIndex,
                                  MeshPass pass) = 0;

    // Default no-op: not every material necessarily supports a wireframe view.
    virtual void setWireframe(bool /*enabled*/) {}

    const vk::Pipeline& getPipeline() const;

    protected:
    core::Logger logger{"Material"};

    vk::Device& device;
    const vk::RenderContext& renderContext;

    std::unique_ptr<vk::Pipeline> pipeline;
    std::unique_ptr<vk::DescriptorSetLayout> descriptorSetLayout;
    std::vector<vk::DescriptorSet> descriptorSets{};

    void allocateDescriptorSets();

    virtual void createPipeline(VkFormat colorFormat, VkFormat depthFormat) = 0;
};

} // namespace nacre::assets
