#pragma once

#include <vulkan/vulkan.h>
#include <nacre/vk/ShaderCache.hpp>
#include <nacre/vk/ShaderModule.hpp>
#include <array>

namespace nacre::vk {

class ShaderEffect {
    public:
    ShaderEffect(ShaderCache& cache, const std::string& vertName, const std::string& fragName)
        : vertModule(cache, vertName, VK_SHADER_STAGE_VERTEX_BIT),
          fragModule(cache, fragName, VK_SHADER_STAGE_FRAGMENT_BIT) {
        logger.print("Created shader effect with vert [{}] and frag [{}]", vertName, fragName);
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> getStages() const {
        return {vertModule.getPipelineInfo(), fragModule.getPipelineInfo()};
    }

    const ShaderModule& getVert() const { return vertModule; }
    const ShaderModule& getFrag() const { return fragModule; }

    private:
    core::Logger logger{"ShaderEffect"};

    ShaderModule vertModule;
    ShaderModule fragModule;
};

} // namespace nacre::vk
