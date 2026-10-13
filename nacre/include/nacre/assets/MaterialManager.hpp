#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/assets/Handles.hpp>
#include <nacre/assets/fwd.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::assets {

class MaterialManager {
    public:
    explicit MaterialManager(const vk::RenderContext& renderContext);
    ~MaterialManager();

    void loadDefaultMaterials();

    std::optional<MaterialHandle> getHandle(const std::string& name);

    Material& getMaterial(const MaterialHandle& handle);

    void setWireframe(bool enabled);

    private:
    core::Logger logger{"MaterialManager"};

    const vk::RenderContext& renderContext;
    uint32_t nextId = 1;

    std::unordered_map<std::string, MaterialHandle> nameToHandleRegistry;
    std::unordered_map<uint32_t, std::unique_ptr<Material>> loadedMaterials;

    MaterialHandle registerMaterial(const std::string& name,
                                    std::unique_ptr<Material> material); // temp
};

} // namespace nacre::assets
