#include <nacre/vk/Image.hpp>
#include <nacre/vk/Check.hpp>

namespace nacre::vk {

Image::Image(VkDevice device, VkImage image, VkFormat format) : device(device), image(image) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vk::check(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "vkCreateImageView");
}

Image::Image(Image&& other) noexcept : device(other.device), image(other.image), imageView(other.imageView) {
    other.imageView = VK_NULL_HANDLE;
}

Image::~Image() {
	// Image cleanup will be handled by the swapchain, so we don't destroy the image here
	// However, we should destroy the image view if it was created
	if (imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device, imageView, nullptr);
	}
}

} // namespace nacre::vk
