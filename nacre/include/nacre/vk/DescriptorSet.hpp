#pragma once

#include <vulkan/vulkan.h>

#include <nacre/vk/Buffer.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class DescriptorSet {
    public:
    DescriptorSet(Device& device, DescriptorPool& pool, VkDescriptorSetLayout layout);
    DescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);
    ~DescriptorSet();

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    DescriptorSet(DescriptorSet&&) noexcept;
    DescriptorSet& operator=(DescriptorSet&&) noexcept;

    void writeBuffer(uint32_t binding, const Buffer& buffer,
                     VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    VkDescriptorSet get() const { return set; }

    private:
    core::Logger logger{"DescriptorSet"};

    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};

} // namespace nacre::vk
