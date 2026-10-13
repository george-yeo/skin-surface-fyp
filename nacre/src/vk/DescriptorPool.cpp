#include <nacre/vk/DescriptorPool.hpp>
#include <nacre/vk/Check.hpp>

#include <nacre/vk/Device.hpp>

namespace nacre::vk {

DescriptorPool::DescriptorPool(Device& device, uint32_t maxSets,
                               const std::vector<VkDescriptorPoolSize>& sizes)
    : device(device) {
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.maxSets = maxSets;
    createInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    createInfo.pPoolSizes = sizes.data();
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    logger.print("Creating descriptor pool with max sets {} and {} pool sizes", maxSets,
                 sizes.size());

    vk::check(vkCreateDescriptorPool(device.get(), &createInfo, nullptr, &pool), "vkCreateDescriptorPool");
}

DescriptorPool::~DescriptorPool() {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device.get(), pool, nullptr);
    }
}

} // namespace nacre::vk
