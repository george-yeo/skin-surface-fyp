#pragma once

#include <string_view>

#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>

namespace nacre::vk {

// Every VkResult outside destructors goes through this; any failure is fatal.
inline void check(VkResult result, std::string_view what) {
    if (result != VK_SUCCESS) {
        static const core::Logger logger{"Vulkan"};
        logger.fatal("{} failed: VkResult {}", what, static_cast<int>(result));
    }
}

} // namespace nacre::vk
