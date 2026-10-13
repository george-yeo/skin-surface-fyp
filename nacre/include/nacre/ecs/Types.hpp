#pragma once

#include <bitset>
#include <cstdint>

namespace nacre::ecs {

using Entity = uint32_t;
using IndexType = size_t;
using Signature = std::bitset<32>;

// Entity layout: lower 16 bits = index, upper 16 bits = generation
constexpr unsigned INDEX_BITS = 16u;
constexpr Entity INDEX_MASK = (1u << INDEX_BITS) - 1u;

constexpr Entity NULL_ENTITY = 0u;
constexpr size_t MAX_ENTITIES = 1u << INDEX_BITS; // the whole 16-bit index space
constexpr IndexType MAX_COMPONENTS = 32u;

inline constexpr Entity makeEntity(uint32_t index, uint32_t generation) {
    return (static_cast<Entity>(generation) << INDEX_BITS) | (index & INDEX_MASK);
}

inline constexpr uint32_t entityIndex(Entity e) {
    return static_cast<uint32_t>(e & INDEX_MASK);
}
inline constexpr uint32_t entityGeneration(Entity e) {
    return static_cast<uint32_t>(e >> INDEX_BITS);
}

inline constexpr bool isValidEntity(Entity e) {
    uint32_t idx = entityIndex(e);
    return e != NULL_ENTITY && idx < MAX_ENTITIES;
}

} // namespace nacre::ecs
