#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class DescriptorPool {
    public:
    DescriptorPool(Device& device, uint32_t maxSets,
                   const std::vector<VkDescriptorPoolSize>& sizes);
    ~DescriptorPool();

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    DescriptorPool(DescriptorPool&&) = delete;
    DescriptorPool& operator=(DescriptorPool&&) = delete;

    VkDescriptorPool get() const { return pool; }

    private:
    core::Logger logger{"DescriptorPool"};

    Device& device;

    VkDescriptorPool pool = VK_NULL_HANDLE;
};

} // namespace nacre::vk
