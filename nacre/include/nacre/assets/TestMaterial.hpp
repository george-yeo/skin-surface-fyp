#pragma once

#include <glm/glm.hpp>

#include <nacre/assets/Material.hpp>

namespace nacre::assets {

class TestMaterial : public Material {
    public:
    explicit TestMaterial(const vk::RenderContext& renderContext);
    ~TestMaterial() override;

    VkPipelineLayout bind(vk::CommandBuffer& commandBuffer, uint32_t frameIndex,
                          MeshPass pass) override;
    void setWireframe(bool enabled) override;

    private:
    std::vector<vk::Buffer> ubos;
    std::unique_ptr<vk::ShaderEffect> shaderEffect;
    // Same layout as `pipeline`; blended, no depth write, back faces culled.
    std::unique_ptr<vk::Pipeline> transparentPipeline;
    bool wireframe = false;
    bool pendingWireframe = false;

    void createDescriptorSetLayout();
    void updateDescriptorSets();

    void createPipeline(VkFormat colorFormat, VkFormat depthFormat) override;
};

} // namespace nacre::assets
