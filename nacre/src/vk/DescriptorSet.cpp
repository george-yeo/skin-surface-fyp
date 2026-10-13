#include <nacre/vk/DescriptorSet.hpp>

#include <nacre/vk/DescriptorPool.hpp>
#include <nacre/vk/Device.hpp>

namespace nacre::vk {

DescriptorSet::DescriptorSet(Device& device, DescriptorPool& pool, VkDescriptorSetLayout layout)
    : DescriptorSet(device.get(), pool.get(), layout) {}

DescriptorSet::DescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
    : device(device), pool(pool) {
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(device, &info, &set) != VK_SUCCESS) {
        logger.fatal("Failed to allocate descriptor set");
    }
}

// The pool is created with FREE_DESCRIPTOR_SET_BIT; every owner is destroyed before it.
DescriptorSet::~DescriptorSet() {
    if (set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, pool, 1, &set);
    }
}

DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
    : device(other.device), pool(other.pool), set(other.set) {
    other.set = VK_NULL_HANDLE;
}

DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept {
    if (this != &other) {
        if (set != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device, pool, 1, &set);
        }
        device = other.device;
        pool = other.pool;
        set = other.set;
        other.set = VK_NULL_HANDLE;
    }
    return *this;
}

void DescriptorSet::writeBuffer(uint32_t binding, const Buffer& buffer, VkDescriptorType type) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer.get();
    bufferInfo.offset = 0;
    bufferInfo.range = buffer.getSize();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

} // namespace nacre::vk
