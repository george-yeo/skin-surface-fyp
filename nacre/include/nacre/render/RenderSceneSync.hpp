#pragma once

#include <nacre/ecs/fwd.hpp>
#include <nacre/render/fwd.hpp>
#include <nacre/scene/fwd.hpp>
#include <nacre/shaders/CameraData.hpp>

namespace nacre::render {

void syncRenderScene(const ecs::ECS& ecs, RenderScene& renderScene);

shaders::CameraData toCameraData(const scene::Camera& camera);

} // namespace nacre::render
