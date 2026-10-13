#include <nacre/vk/DescriptorSetLayout.hpp>

namespace nacre::vk {

DescriptorSetLayout::DescriptorSetLayout(VkDevice device,
                                         const std::vector<VkDescriptorSetLayoutBinding>& bindings)
    : device(device) {
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        logger.fatal("Failed to create DescriptorSetLayout");
    }
    logger.print("DescriptorSetLayout created");
}

DescriptorSetLayout::~DescriptorSetLayout() {
    if (layout != VK_NULL_HANDLE) {
        logger.print("Destroying DescriptorSetLayout");
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
}

DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
    : layout(other.layout), device(other.device) {
    other.layout = VK_NULL_HANDLE;
}

DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept {
    if (this != &other) {
        if (layout != VK_NULL_HANDLE) {
            logger.print("Destroying DescriptorSetLayout");
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
        layout = other.layout;
        other.layout = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace nacre::vk
