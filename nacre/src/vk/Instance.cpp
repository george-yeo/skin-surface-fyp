#include <nacre/vk/Instance.hpp>
#include <nacre/vk/Check.hpp>
#include <nacre/core/Logger.hpp>

#include <cstring>
#include <vector>

namespace nacre::vk {

Instance::Instance(std::span<const char* const> extensions) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "PhysicsEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    // Debug only, and only if installed: without the Vulkan SDK the layer is
    // missing and instance creation would fail.
    static const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    uint32_t layerCount = 0;
    vk::check(vkEnumerateInstanceLayerProperties(&layerCount, nullptr), "vkEnumerateInstanceLayerProperties");
    std::vector<VkLayerProperties> available(layerCount);
    vk::check(vkEnumerateInstanceLayerProperties(&layerCount, available.data()), "vkEnumerateInstanceLayerProperties");

    bool hasValidation = false;
    for (const VkLayerProperties& layer : available) {
        if (std::strcmp(layer.layerName, validationLayer) == 0) {
            hasValidation = true;
            break;
        }
    }
    if (hasValidation) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
    } else {
        logger.warn("{} not available; running without validation", validationLayer);
    }
#endif

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        logger.fatal("Failed to create Vulkan instance");
    }

    logger.print("Vulkan instance created");
}

Instance::~Instance() {
    vkDestroyInstance(instance, nullptr);
    logger.print("Vulkan instance destroyed");
}

} // namespace nacre::vk
