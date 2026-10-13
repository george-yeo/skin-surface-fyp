#pragma once

#include <vector>

#include <nacre/render/FrameStage.hpp>
#include <nacre/assets/MeshPass.hpp>
#include <nacre/shaders/ObjectData.hpp>

namespace nacre::render {

// Opaque: clears, uploads the object data, draws opaque meshes.
// Transparent: runs after the impostors, draws the rest over them.
class SceneStage : public FrameStage {
    public:
    explicit SceneStage(assets::MeshPass pass) : pass(pass) {}

    void render(const FrameContext& context) override;

    private:
    assets::MeshPass pass;
    std::vector<shaders::ObjectData> objectData; // reused across frames
};

} // namespace nacre::render
