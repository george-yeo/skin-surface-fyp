#pragma once

#include <nacre/ecs/Types.hpp>
#include <unordered_set>

namespace nacre::ecs {

class System {
    public:
    virtual ~System() = default;
    virtual void update(double dt) = 0;
    virtual std::unordered_set<Entity>& getEntities() = 0;
};

} // namespace nacre::ecs
