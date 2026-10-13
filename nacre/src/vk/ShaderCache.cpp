#include <fstream>
#include <ios>
#include <string>

#include <nacre/vk/Device.hpp>
#include <nacre/vk/ShaderCache.hpp>

namespace nacre::vk {

// [moduleName, filePath]
const std::unordered_map<std::string, std::string> MODULE_REGISTRY = {
    {"test_shader.vert", "test_shader.vert"},
    {"test_shader.frag", "test_shader.frag"},
};

ShaderCache::ShaderCache(Device& device) : device(device) {

}

ShaderCache::~ShaderCache() {
    logger.print("Destroying shader cache with {} modules", cache.size());
    
    for (auto& [path, module] : cache) {
        vkDestroyShaderModule(device.get(), module, nullptr);
    }
}

VkShaderModule ShaderCache::get(const std::string& moduleName) {
    auto it = cache.find(moduleName);
    if (it != cache.end()) {
        return it->second;
    }

    VkShaderModule module = compile(moduleName);
    cache[moduleName] = module;
    return module;
}

std::vector<char> ShaderCache::readSpriv(const std::string& path) {
    const std::string finalPath = std::string(NACRE_SHADER_DIR) + "/" + path + ".spv";

    std::ifstream file(finalPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        logger.fatal("Failed to open shader file: {}", finalPath);
    }

    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);

    return buffer;
}

VkShaderModule ShaderCache::compile(const std::string& path) {
    logger.print("Compiling shader {}", path);
    
    auto code = readSpriv(path);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device.get(), &info, nullptr, &shaderModule) != VK_SUCCESS) {
        logger.fatal("Failed to create shader module for {}", path);
    }

    return shaderModule;
}

} // namespace nacre::vk
