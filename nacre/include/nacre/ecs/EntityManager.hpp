#pragma once

#include <array>
#include <nacre/ecs/Types.hpp>
#include <nacre/core/Logger.hpp>
#include <queue>
#include <vector>

namespace nacre::ecs {

class EntityManager {
    public:
    EntityManager();
    ~EntityManager() = default;

    Entity create();
    void destroy(Entity);

    void setSignature(Entity, Signature);
    Signature getSignature(Entity);
    bool isEntityAlive(Entity e) const {
        if (!isValidEntity(e))
            return false;
        uint32_t idx = entityIndex(e);
        // Handles only carry the low 16 bits of the generation.
        return alive[idx] && makeEntity(idx, generations[idx]) == e;
    }

    std::vector<Entity> getEntitiesOfSignature(Signature signature) const;

    private:
    core::Logger logger{"EntityManager"};
    uint32_t nextIndex = 1; // start from 1 to reserve 0 as NULL_ENTITY

    std::queue<uint32_t> availableIndices;
    std::array<Signature, MAX_ENTITIES> signatures;
    std::array<uint32_t, MAX_ENTITIES> generations{};
    std::array<bool, MAX_ENTITIES> alive{};
};

} // namespace nacre::ecs
