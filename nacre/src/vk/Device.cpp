#include <nacre/vk/CommandPool.hpp>
#include <nacre/vk/Check.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/Instance.hpp>
#include <nacre/vk/Surface.hpp>

#include <nacre/core/Logger.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace nacre::vk {

struct Device::Impl {
    Impl(Device& owner, Instance& instance, VkSurfaceKHR surface)
        : owner(&owner),
          surface(surface),
          instance(instance),
          logger("Vulkan Device") {}

    ~Impl() {
        destroyUploadCommandPool();
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            logger.print("Vulkan device destroyed");
        }
    }

    Device* owner;
    VkSurfaceKHR surface;
    Instance& instance;

    core::Logger logger;

    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    std::unique_ptr<CommandPool> uploadCommandPool;

    uint32_t graphicsFamilyIndex = 0;
    uint32_t presentFamilyIndex = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    QueueFamilyIndices findQueueFamilies() {
        QueueFamilyIndices indices;

        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, families.data());

        // First graphics family; present prefers that same family.
        for (uint32_t i = 0; i < count; i++) {
            if (!indices.graphics && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                indices.graphics = i;

            VkBool32 presentSupport = VK_FALSE;
            vk::check(vkGetPhysicalDeviceSurfaceSupportKHR(
                physicalDevice, i, surface, &presentSupport), "vkGetPhysicalDeviceSurfaceSupportKHR");
            if (presentSupport && (!indices.present || indices.graphics == i))
                indices.present = i;

            if (indices.isComplete())
                break;
        }

        return indices;
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies();

        graphicsFamilyIndex = indices.graphics.value();
        presentFamilyIndex = indices.present.value();

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (uint32_t family : {graphicsFamilyIndex, presentFamilyIndex}) {
            if (!queueInfos.empty() && queueInfos.front().queueFamilyIndex == family) {
                continue;
            }
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;
            queueInfos.push_back(queueInfo);
        }

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features13;
        features2.features.fillModeNonSolid = VK_TRUE;
        features2.features.geometryShader = VK_TRUE;

        const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &features2;
        deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        deviceInfo.pQueueCreateInfos = queueInfos.data();
        deviceInfo.enabledExtensionCount =
            static_cast<uint32_t>(sizeof(deviceExts) / sizeof(deviceExts[0]));
        deviceInfo.ppEnabledExtensionNames = deviceExts;

        vk::check(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device), "vkCreateDevice");

        vkGetDeviceQueue(device, graphicsFamilyIndex, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentFamilyIndex, 0, &presentQueue);

        logger.print("Vulkan device created");
    }

    void pickPhysicalDevice() {
        uint32_t count;
        vk::check(vkEnumeratePhysicalDevices(instance.get(), &count, nullptr), "vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count);
        vk::check(vkEnumeratePhysicalDevices(instance.get(), &count, devices.data()), "vkEnumeratePhysicalDevices");

        // Just pick the first one
        physicalDevice = devices[0];
    }

    void createUploadCommandPool() {
        uploadCommandPool = std::make_unique<CommandPool>(*owner, graphicsFamilyIndex);
    }
    void destroyUploadCommandPool() { uploadCommandPool.reset(); }

    VkCommandBuffer beginOneTimeCommand() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = uploadCommandPool->get();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vk::check(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "vkAllocateCommandBuffers");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vk::check(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");

        return cmd;
    }

    void endOneTimeCommand(VkQueue queue, VkCommandBuffer cmd) {
        vk::check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        vk::check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit");
        vk::check(vkQueueWaitIdle(queue), "vkQueueWaitIdle");

        vkFreeCommandBuffers(device, uploadCommandPool->get(), 1, &cmd);
    }
};

Device::Device(Instance& instance, const Surface& surface)
    : impl(std::make_unique<Impl>(*this, instance, surface.get())) {
    impl->pickPhysicalDevice();
    impl->createLogicalDevice();
    impl->createUploadCommandPool();
}

Device::~Device() = default;

QueueFamilyIndices Device::findQueueFamilies() {
    return impl->findQueueFamilies();
}

void Device::createLogicalDevice() {
    impl->createLogicalDevice();
}

void Device::pickPhysicalDevice() {
    impl->pickPhysicalDevice();
}

void Device::createUploadCommandPool() {
    impl->createUploadCommandPool();
}

void Device::destroyUploadCommandPool() {
    impl->destroyUploadCommandPool();
}

VkCommandBuffer Device::beginOneTimeCommand() {
    return impl->beginOneTimeCommand();
}

void Device::endOneTimeCommand(VkQueue queue, VkCommandBuffer cmd) {
    impl->endOneTimeCommand(queue, cmd);
}

const Instance& Device::getInstance() const {
    return impl->instance;
}
const VkDevice& Device::get() const {
    return impl->device;
}
VkPhysicalDevice Device::getPhysicalDevice() const {
    return impl->physicalDevice;
}
VkQueue Device::getGraphicsQueue() const {
    return impl->graphicsQueue;
}
VkQueue Device::getPresentQueue() const {
    return impl->presentQueue;
}
uint32_t Device::getGraphicsFamilyIndex() const {
    return impl->graphicsFamilyIndex;
}
uint32_t Device::getPresentFamilyIndex() const {
    return impl->presentFamilyIndex;
}

} // namespace nacre::vk
