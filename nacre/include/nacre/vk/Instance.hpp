#pragma once

#include <nacre/core/Logger.hpp>
#include <span>
#include <vulkan/vulkan.h>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class Instance {
    public:
    explicit Instance(std::span<const char* const> extensions);
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    VkInstance get() const { return instance; }

    private:
    core::Logger logger{"Vulkan Instance"};

    VkInstance instance;
};

} // namespace nacre::vk
