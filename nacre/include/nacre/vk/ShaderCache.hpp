#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class ShaderCache {
    public:
    ShaderCache(Device& device);
    ~ShaderCache();

    VkShaderModule get(const std::string& moduleName);

    private:
    core::Logger logger{"ShaderCache"};

    Device& device;
    std::unordered_map<std::string, VkShaderModule> cache;

    std::vector<char> readSpriv(const std::string& path);
    VkShaderModule compile(const std::string& path);
};

} // namespace nacre::vk
