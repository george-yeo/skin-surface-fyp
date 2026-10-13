#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include <nacre/core/Logger.hpp>

namespace nacre::vk {

class DescriptorSetLayout {
    public:
    DescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    ~DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    DescriptorSetLayout(DescriptorSetLayout&&) noexcept;
    DescriptorSetLayout& operator=(DescriptorSetLayout&&) noexcept;

    VkDescriptorSetLayout get() const { return layout; }

    private:
    core::Logger logger{"DescriptorSetLayout"};

    VkDevice device;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};

} // namespace nacre::vk
