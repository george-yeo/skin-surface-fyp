#pragma once

#include <nacre/ecs/ComponentManager.hpp>
#include <nacre/ecs/EntityManager.hpp>
#include <nacre/ecs/SystemManager.hpp>

namespace nacre::ecs {

class ECS {
    public:
    ECS();
    ~ECS() = default;

    Entity createEntity();
    void destroyEntity(Entity);

    template <typename T>
    void registerComponent() {
        componentManager->registerComponent<T>();
    }

    template <typename T>
    void addComponent(Entity entity, T& component) {
        logger.check(entityManager->isEntityAlive(entity), "addComponent: stale entity {}", entity);
        componentManager->addComponent<T>(entity, component);

        Signature signature = entityManager->getSignature(entity);
        signature.set(componentManager->getComponentBit<T>());

        entityManager->setSignature(entity, signature);
        systemManager->onEntitySignatureChanged(entity, signature);
    }

    template <typename T>
    void removeComponent(Entity entity) {
        logger.check(entityManager->isEntityAlive(entity), "removeComponent: stale entity {}",
                     entity);
        componentManager->removeComponent<T>(entity);

        Signature signature = entityManager->getSignature(entity);
        signature.reset(componentManager->getComponentBit<T>());

        entityManager->setSignature(entity, signature);
        systemManager->onEntitySignatureChanged(entity, signature);
    }

    template <typename T>
    void registerSystem() {
        // does not update existing entities
        systemManager->registerSystem<T>();
    }

    template <typename T>
    void setSystemSignature(Signature signature) {
        // does not update existing entities
        systemManager->setSignature<T>(signature);
    }

    template <typename T, typename S>
    void setSystemSignature() {
        // does not update existing entities
        Signature signature = systemManager->getSignature<T>();
        signature.set(componentManager->getComponentBit<S>());
        systemManager->setSignature<T>(signature);
    }

    template <typename... Components>
    std::vector<Entity> view() const {
        Signature signature;
        (signature.set(componentManager->getComponentBit<Components>()), ...); // fold expression
        return entityManager->getEntitiesOfSignature(signature);
    }

    template <typename T>
    T& getComponent(Entity entity) const {
        return componentManager->getComponent<T>(entity);
    }

    template <typename T>
    bool hasComponent(Entity entity) const {
        return entityManager->isEntityAlive(entity) && componentManager->hasComponent<T>(entity);
    }

    void update(double dt);

    private:
    core::Logger logger{"ECS"};

    std::unique_ptr<EntityManager> entityManager;
    std::unique_ptr<ComponentManager> componentManager;
    std::unique_ptr<SystemManager> systemManager;
};

} // namespace nacre::ecs
