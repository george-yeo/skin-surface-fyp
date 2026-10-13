#include "skin/vis/EntityLayer.hpp"

#include <nacre/ecs/ECS.hpp>
#include <nacre/assets/MeshManager.hpp>

namespace skin::vis {

void EntityLayer::destroyAll(nacre::ecs::ECS& ecs, nacre::assets::MeshManager& meshManager) {
    for (nacre::ecs::Entity entity : entities) {
        ecs.destroyEntity(entity);
    }
    entities.clear();

    // Release after the entities are gone; MeshManager defers the GPU free
    // until the mesh is no longer referenced by an in-flight frame.
    for (const std::string& name : meshes) {
        if (auto handle = meshManager.getHandle(name)) {
            meshManager.releaseHandle(*handle);
        }
    }
    meshes.clear();
}

} // namespace skin::vis
