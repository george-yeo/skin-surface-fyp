#include <nacre/ecs/ECS.hpp>

namespace nacre::ecs {

ECS::ECS() {
    entityManager = std::make_unique<EntityManager>();
    componentManager = std::make_unique<ComponentManager>();
    systemManager = std::make_unique<SystemManager>();

    logger.print("ECS created");
}

Entity ECS::createEntity() {
    Entity entity = entityManager->create();
    systemManager->onEntitySignatureChanged(entity, Signature{});

    return entity;
}

void ECS::destroyEntity(Entity entity) {
    // A stale handle's index may belong to a newer entity by now.
    if (!entityManager->isEntityAlive(entity)) {
        logger.warn("destroyEntity: stale or invalid entity {}", entity);
        return;
    }
    entityManager->destroy(entity);
    componentManager->onEntityDestroyed(entity);
    systemManager->onEntityDestroyed(entity);
}

void ECS::update(double dt) {
    systemManager->update(dt);
}

} // namespace nacre::ecs
