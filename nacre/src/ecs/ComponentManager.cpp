#include <nacre/ecs/ComponentManager.hpp>

#include <memory>

namespace nacre::ecs {

void ComponentManager::onEntityDestroyed(Entity entity) {
    for (auto& [type, storage] : storages) {
        storage->onEntityDestroyed(entity);
    }
}

} // namespace nacre::ecs
