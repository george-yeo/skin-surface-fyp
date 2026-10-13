#include <nacre/vk/DepthImage.hpp>
#include <nacre/vk/Check.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/Image.hpp>
#include <nacre/vk/Swapchain.hpp>

#include <nacre/core/Logger.hpp>

namespace nacre::vk {

struct Swapchain::Impl {
    Impl(Device& device, VkSurfaceKHR surface, VkSwapchainKHR oldSwapchain)
        : device(device), surface(surface), logger("Swapchain") {
        initialize(oldSwapchain);
    }

    ~Impl() {
        for (VkSemaphore semaphore : renderFinishedSemaphores) {
            vkDestroySemaphore(device.get(), semaphore, nullptr);
        }
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device.get(), swapchain, nullptr);
        }
    }

    void initialize(VkSwapchainKHR oldSwapchain) {
        auto physicalDevice = device.getPhysicalDevice();

        VkSurfaceCapabilitiesKHR capabilities;
        vk::check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

        // Pick format(prefer BGRA8 SRGB)
        uint32_t formatCount = 0;
        vk::check(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vk::check(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");

        VkSurfaceFormatKHR chosenFormat = formats[0]; // fallback
        for (auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosenFormat = f;
                break;
            }
        }

        // Pick depth format (prefer D32 SRGB)
        const std::vector<VkFormat> candidates = {
            VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

        VkFormat chosenDepthFormat = candidates[0]; // fallback
        for (VkFormat f : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, f, &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                chosenDepthFormat = f;
                break;
            }
        }

        // Pick present mode (prefer mailbox > fifo)
        uint32_t modeCount = 0;
        vk::check(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR");
        std::vector<VkPresentModeKHR> modes(modeCount);
        vk::check(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount,
                                                  modes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR");

        VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed to exist
        for (auto& m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                chosenMode = m;
                break;
            }
        }

        extent = capabilities.currentExtent;

        VkSwapchainCreateInfoKHR swapInfo{};
        swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapInfo.surface = surface;
        swapInfo.minImageCount = capabilities.minImageCount;
        swapInfo.imageFormat = chosenFormat.format;
        swapInfo.imageColorSpace = chosenFormat.colorSpace;
        swapInfo.imageExtent = extent;
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapInfo.preTransform = capabilities.currentTransform;
        swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapInfo.presentMode = chosenMode;
        swapInfo.clipped = VK_TRUE;
        swapInfo.oldSwapchain = oldSwapchain;

        format = chosenFormat.format;
        depthFormat = chosenDepthFormat;

        vk::check(vkCreateSwapchainKHR(device.get(), &swapInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        // Retrieve the images
        uint32_t imageCount = 0;
        vk::check(vkGetSwapchainImagesKHR(device.get(), swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
        std::vector<VkImage> swapImages(imageCount);
        vk::check(vkGetSwapchainImagesKHR(device.get(), swapchain, &imageCount, swapImages.data()), "vkGetSwapchainImagesKHR");

        depthImages.clear();

        for (auto image : swapImages) {
            images.emplace_back(device.get(), image, chosenFormat.format);
            depthImages.emplace_back(device, chosenDepthFormat, extent);
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        renderFinishedSemaphores.resize(imageCount, VK_NULL_HANDLE);
        for (VkSemaphore& semaphore : renderFinishedSemaphores) {
            if (vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
                logger.fatal("Failed to create render-finished semaphore");
            }
        }

        if (oldSwapchain == VK_NULL_HANDLE) {
            logger.print("Swapchain created with {} images", imageCount);
        } else {
            logger.print("Swapchain re-created with {} images", imageCount);
        }
    }

    Device& device;
    VkSurfaceKHR surface;
    core::Logger logger;

    VkExtent2D extent{};
    VkFormat format{};
    VkFormat depthFormat{};

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<Image> images;
    std::vector<DepthImage> depthImages;
    std::vector<VkSemaphore> renderFinishedSemaphores;
};

Swapchain::Swapchain(Device& device, VkSurfaceKHR surface, VkSwapchainKHR oldSwapchain)
    : impl(std::make_unique<Impl>(device, surface, oldSwapchain)) {}

Swapchain::~Swapchain() {
    impl->depthImages.clear();
};

VkSwapchainKHR Swapchain::get() const {
    return impl->swapchain;
}

const std::vector<Image>& Swapchain::getImages() const {
    return impl->images;
}
std::vector<Image>& Swapchain::getImages() {
    return impl->images;
}
const std::vector<DepthImage>& Swapchain::getDepthImages() const {
    return impl->depthImages;
}
std::vector<DepthImage>& Swapchain::getDepthImages() {
    return impl->depthImages;
}
VkFormat Swapchain::getFormat() const {
    return impl->format;
}
VkFormat Swapchain::getDepthFormat() const {
    return impl->depthFormat;
}
size_t Swapchain::getImageCount() const {
    return impl->images.size();
}
VkExtent2D Swapchain::getExtent() const {
    return impl->extent;
}
VkSemaphore Swapchain::getRenderFinishedSemaphore(uint32_t imageIndex) const {
    return impl->renderFinishedSemaphores[imageIndex];
}

} // namespace nacre::vk
