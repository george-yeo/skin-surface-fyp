#include <nacre/assets/MaterialManager.hpp>


#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/assets/Material.hpp>
#include <nacre/assets/TestMaterial.hpp>

namespace nacre::assets {

MaterialManager::MaterialManager(const vk::RenderContext& renderContext)
    : renderContext(renderContext) {}

void MaterialManager::loadDefaultMaterials() {
    registerMaterial("test", std::make_unique<TestMaterial>(renderContext));
}

MaterialManager::~MaterialManager() {
    loadedMaterials.clear();
}

MaterialHandle MaterialManager::registerMaterial(const std::string& name,
                                                 std::unique_ptr<Material> material) {
    if (nameToHandleRegistry.contains(name)) {
        logger.fatal("registerMaterial: name '{}' already registered", name);
    }

    MaterialHandle handle{nextId++};
    loadedMaterials.emplace(handle.id, std::move(material));
    nameToHandleRegistry.insert({name, handle});

    logger.print("Registered material '{}' with handle ID {}", name, handle.id);
    return handle;
}

std::optional<MaterialHandle> MaterialManager::getHandle(const std::string& name) {
    auto it = nameToHandleRegistry.find(name);

    if (it == nameToHandleRegistry.end()) {
        return std::nullopt;
    }

    return it->second;
}

Material& MaterialManager::getMaterial(const MaterialHandle& handle) {
    auto it = loadedMaterials.find(handle.id);
    if (it == loadedMaterials.end() || it->second == nullptr) {
        logger.fatal("getMaterial: material handle {} not found", handle.id);
    }

    return *it->second;
}


void MaterialManager::setWireframe(bool enabled) {
    for (auto& [id, material] : loadedMaterials) {
        material->setWireframe(enabled);
    }
}

} // namespace nacre::assets
