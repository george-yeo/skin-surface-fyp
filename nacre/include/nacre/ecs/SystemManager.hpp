#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>

#include <nacre/ecs/System.hpp>
#include <nacre/ecs/Types.hpp>
#include <nacre/core/Logger.hpp>

namespace nacre::ecs {

class SystemManager {
    public:
    template <typename T, typename... Args>
    void registerSystem(Args&&... args) {
        std::type_index type = typeid(T);
        if (systems.find(type) != systems.end()) {
            logger.fatal("System {} already registered", type.name());
        }

        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        systems[type] = std::move(system); // upcast via move

        logger.print("System {} registered", type.name());
    }

    template <typename T>
    T& getSystem() {
        std::type_index type = typeid(T);
        if (systems.find(type) == systems.end()) {
            logger.fatal("System {} not registered", type.name());
        }

        return *static_cast<T*>(systems[type].get());
    }

    template <typename T>
    Signature getSignature() {
        std::type_index type = typeid(T);
        if (systems.find(type) == systems.end()) {
            logger.fatal("System {} not registered", type.name());
        }

        return signatures[type];
    }

    template <typename T>
    void setSignature(Signature signature) {
        std::type_index type = typeid(T);
        if (systems.find(type) == systems.end()) {
            logger.fatal("System {} not registered", type.name());
        }

        signatures[type] = signature;
    }

    void onEntityDestroyed(Entity entity);
    void onEntitySignatureChanged(Entity entity, Signature entitySignature);

    void update(double dt);

    private:
    core::Logger logger{"SystemManager"};

    std::unordered_map<std::type_index, Signature> signatures;
    std::unordered_map<std::type_index, std::unique_ptr<System>> systems;
};

} // namespace nacre::ecs
