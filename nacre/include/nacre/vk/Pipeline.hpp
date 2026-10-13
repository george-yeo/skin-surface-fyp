#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class Pipeline {
    public:
    struct Config {
        VkFormat colorFormat;
        VkFormat depthFormat;

        VkPolygonMode polygonMode = VK_POLYGON_MODE_LINE;
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
        VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

        bool depthTest = true;
        bool depthWrite = true;
        bool alphaBlend = false; // straight alpha: src * a + dst * (1 - a)
        VkCompareOp depthOp = VK_COMPARE_OP_LESS;

        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttributes;

        std::vector<VkDescriptorSetLayout> setLayouts;
        std::vector<VkPushConstantRange> pushConstants;
    };

    Pipeline(VkDevice device, ShaderEffect& effect, const Config& config);
    ~Pipeline();

    VkPipeline get() const { return pipeline; }
    VkPipelineLayout getLayout() const { return pipelineLayout; }

    private:
    core::Logger logger{"Pipeline"};

    VkDevice device;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

} // namespace nacre::vk
