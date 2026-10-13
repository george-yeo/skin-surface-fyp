#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <nacre/ecs/Types.hpp>
#include <nacre/core/Logger.hpp>
#include <optional>
#include <vector>

#include <unordered_map>

namespace nacre::ecs {

class BaseComponentStorage {
    public:
    virtual ~BaseComponentStorage() = default;
    virtual void onEntityDestroyed(Entity entity) = 0;
};

template <typename T>
class ComponentStorage : public BaseComponentStorage {
    public:
    ComponentStorage() {
        indexToActivePos.fill(npos);
        activeEntities.reserve(64);
    }

    void insert(Entity entity, const T& component) {
        logger.check(isValidEntity(entity), "insert: invalid entity {}", entity);
        uint32_t idx = entityIndex(entity);
        if (idx >= MAX_ENTITIES)
            return;
        if (!data[idx].has_value()) {
            // new component for this entity
            data[idx] = component;
            indexToActivePos[idx] = activeEntities.size();
            activeEntities.push_back(entity);
        } else {
            // update existing
            data[idx] = component;
        }
    }

    void remove(Entity entity) {
        logger.check(isValidEntity(entity), "remove: invalid entity {}", entity);
        uint32_t idx = entityIndex(entity);
        if (idx >= MAX_ENTITIES)
            return;
        if (!data[idx].has_value())
            return;

        size_t pos = indexToActivePos[idx];
        size_t lastPos = activeEntities.size() - 1;
        if (pos != lastPos) {
            Entity lastEntity = activeEntities[lastPos];
            activeEntities[pos] = lastEntity;
            indexToActivePos[entityIndex(lastEntity)] = pos;
        }
        activeEntities.pop_back();
        data[idx].reset();
        indexToActivePos[idx] = npos;
    }

    T& get(Entity entity) {
        logger.check(isValidEntity(entity), "get: invalid entity {}", entity);
        uint32_t idx = entityIndex(entity);
        return *data[idx];
    }

    bool has(Entity entity) const {
        if (!isValidEntity(entity))
            return false;
        uint32_t idx = entityIndex(entity);
        if (idx >= MAX_ENTITIES)
            return false;
        return data[idx].has_value();
    }

    void onEntityDestroyed(Entity entity) override { remove(entity); }

    private:
    static constexpr size_t npos = static_cast<size_t>(-1);
    std::array<std::optional<T>, MAX_ENTITIES> data;
    std::array<size_t, MAX_ENTITIES> indexToActivePos;
    std::vector<Entity> activeEntities;
    core::Logger logger{"ComponentStorage"};

    public:
    // Accessor for dense active entities for iteration
    const std::vector<Entity>& getActiveEntities() const { return activeEntities; }
};

} // namespace nacre::ecs
