#include <nacre/ecs/SystemManager.hpp>

namespace nacre::ecs {

void SystemManager::onEntityDestroyed(Entity entity) {
    for (auto& [type, system] : systems) {
        system->getEntities().erase(entity);
    }
}

void SystemManager::onEntitySignatureChanged(Entity entity, Signature entitySignature) {
    for (auto& [type, system] : systems) {
        Signature systemSignature = signatures[type];

        if ((entitySignature & systemSignature) == systemSignature) {
            system->getEntities().insert(entity);
        } else {
            system->getEntities().erase(entity);
        }
    }
}

void SystemManager::update(double dt) {
    for (auto& system : systems) {
        system.second->update(dt);
    }
}

} // namespace nacre::ecs
