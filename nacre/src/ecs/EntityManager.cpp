#include <nacre/ecs/EntityManager.hpp>

namespace nacre::ecs {

EntityManager::EntityManager() {
    signatures.fill(Signature{});
}

Entity EntityManager::create() {
    uint32_t index;
    if (!availableIndices.empty()) {
        index = availableIndices.front();
        availableIndices.pop();
    } else {
        if (nextIndex >= MAX_ENTITIES) {
            logger.fatal("Exceeded maximum number of entities");
        }
        index = nextIndex++;
    }

    uint32_t gen = generations[index];
    Entity entity = makeEntity(index, gen);
    alive[index] = true;
    // logger.print("Entity {} created", entity);
    return entity;
}

void EntityManager::destroy(Entity entity) {
    if (!isEntityAlive(entity))
        return;
    uint32_t index = entityIndex(entity);
    signatures[index].reset();
    // bump generation to invalidate old handles
    generations[index]++;
    alive[index] = false;
    availableIndices.push(index);
    // logger.print("Entity {} destroyed", entity);
}

void EntityManager::setSignature(Entity entity, Signature signature) {
    if (!isEntityAlive(entity))
        return;
    signatures[entityIndex(entity)] = signature;
}

Signature EntityManager::getSignature(Entity entity) {
    if (!isEntityAlive(entity))
        return Signature{};
    return signatures[entityIndex(entity)];
}

std::vector<Entity> EntityManager::getEntitiesOfSignature(Signature signature) const {
    std::vector<Entity> entities;
    for (uint32_t i = 0; i < nextIndex; i++) {
        if (!alive[i])
            continue; // skip non-alive indices
        if ((signatures[i] & signature) == signature) {
            entities.push_back(makeEntity(i, generations[i]));
        }
    }
    return entities;
}

} // namespace nacre::ecs
