#pragma once

#include <nacre/ecs/ComponentStorage.hpp>
#include <nacre/ecs/Types.hpp>
#include <nacre/core/Logger.hpp>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace nacre::ecs {

class ComponentManager {
    public:
    template <typename T>
    void registerComponent() {
        std::type_index type = typeid(T);
        if (storages.find(type) != storages.end()) {
            logger.fatal("Component {} already registered", type.name());
        }
        if (nextBit >= MAX_COMPONENTS) {
            logger.fatal("Exceeded maximum number of component types");
        }

        componentBits[type] = nextBit++;
        storages[type] = std::make_unique<ComponentStorage<T>>();

        logger.print("Component {} registered", type.name());
    }

    template <typename T>
    void addComponent(Entity entity, const T& component) {
        logger.check(isValidEntity(entity), "addComponent: invalid entity {}", entity);
        getStorage<T>().insert(entity, component);
    }

    template <typename T>
    void removeComponent(Entity entity) {
        logger.check(isValidEntity(entity), "removeComponent: invalid entity {}", entity);
        getStorage<T>().remove(entity);
    }

    template <typename T>
    T& getComponent(Entity entity) {
        logger.check(isValidEntity(entity), "getComponent: invalid entity {}", entity);
        return getStorage<T>().get(entity);
    }

    template <typename T>
    bool hasComponent(Entity entity) {
        if (!isValidEntity(entity))
            return false;
        return getStorage<T>().has(entity);
    }

    template <typename T>
    IndexType getComponentBit() {
        std::type_index type = typeid(T);
        if (componentBits.find(type) == componentBits.end()) {
            logger.fatal("Component {} not registered", type.name());
        }
        return componentBits[type];
    }

    template <typename T>
    ComponentStorage<T>& getStorage() {
        std::type_index type = typeid(T);
        if (storages.find(type) == storages.end()) {
            logger.fatal("Component {} not registered", type.name());
        }

        return static_cast<ComponentStorage<T>&>(*storages[type]);
    }

    void onEntityDestroyed(Entity entity);

    private:
    core::Logger logger{"ComponentManager"};

    std::unordered_map<std::type_index, std::unique_ptr<BaseComponentStorage>> storages;
    std::unordered_map<std::type_index, IndexType> componentBits;
    IndexType nextBit = 0;
};

} // namespace nacre::ecs
