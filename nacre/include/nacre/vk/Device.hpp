#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <memory>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    // std::optional<uint32_t> compute;
    // std::optional<uint32_t> transfer;

    bool isComplete() { return graphics.has_value() && present.has_value(); }
};

class Device {
    public:
    Device(Instance& instance, const Surface& surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
    
    const Instance& getInstance() const;
    const VkDevice& get() const;
    VkPhysicalDevice getPhysicalDevice() const;

    VkQueue getGraphicsQueue() const;
    VkQueue getPresentQueue() const;
    uint32_t getGraphicsFamilyIndex() const;
    uint32_t getPresentFamilyIndex() const;

    VkCommandBuffer beginOneTimeCommand();
    void endOneTimeCommand(VkQueue queue, VkCommandBuffer cmd);

    private:
    // Pimpl to hide implementation details and heavy headers
    struct Impl;
    std::unique_ptr<Impl> impl;

    QueueFamilyIndices findQueueFamilies();
    void pickPhysicalDevice();
    void createLogicalDevice();

    void createUploadCommandPool();
    void destroyUploadCommandPool();
};

} // namespace nacre::vk
