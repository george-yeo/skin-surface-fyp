#pragma once

#include <nacre/ecs/fwd.hpp>
#include <nacre/render/fwd.hpp>

namespace nacre::render {

class Renderer {
    public:
    virtual ~Renderer() = default;

    virtual void update(const ecs::ECS& ecs, RenderScene& renderScene) = 0;
};

} // namespace nacre::render
