#pragma once

#include <vulkan/vulkan.h>
#include <nacre/vk/ShaderCache.hpp>

namespace nacre::vk {

class ShaderModule {
    public:
    ShaderModule(ShaderCache& cache, const std::string& moduleName, VkShaderStageFlagBits stage);

    VkShaderModule get() const { return cache.get(moduleName); }

    VkPipelineShaderStageCreateInfo getPipelineInfo() const;

    private:
    ShaderCache& cache;
    const std::string moduleName;
    VkShaderStageFlagBits stage;
};

} // namespace nacre::vk
