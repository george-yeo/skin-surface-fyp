#include <nacre/vk/ShaderModule.hpp>

#include <string>

namespace nacre::vk {

ShaderModule::ShaderModule(ShaderCache& cache, const std::string& moduleName,
                           VkShaderStageFlagBits stage)
    : cache(cache), moduleName(moduleName), stage(stage) {
	
}

VkPipelineShaderStageCreateInfo ShaderModule::getPipelineInfo() const {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = get();
    info.pName = "main";
    return info;
}

} // namespace nacre::vk
