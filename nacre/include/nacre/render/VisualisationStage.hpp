#pragma once

#include <memory>
#include <vector>

#include <nacre/core/Logger.hpp>
#include <nacre/render/FrameStage.hpp>
#include <nacre/vk/fwd.hpp>
#include <nacre/render/fwd.hpp>

namespace nacre::render {

// Renders Balls = {position, radius} and Lines = {start, end, thickness, color} as
// instanced quads rather than ray-marching a fullscreen triangle: each ball
// gets a camera-facing billboard quad, each line gets a quad oriented along
// its segment (both sized to bound the primitive plus its radius). Every
// quad is a cheap rasterizer-culled proxy -- its own fragment shader
// ray-intersects just that one sphere/capsule (radius = thickness / 2) to
// find the exact analytic surface, writing real gl_FragDepth so it
// composites correctly with rasterized geometry. Ball/line occlusion
// between the two draws is resolved by the ordinary depth test, not any
// manual per-pixel bookkeeping.
class VisualisationStage : public FrameStage {
    public:
    explicit VisualisationStage(const vk::RenderContext& renderContext);
    ~VisualisationStage() override;

    VisualisationStage(const VisualisationStage&) = delete;
    VisualisationStage& operator=(const VisualisationStage&) = delete;

    VisualisationStage(VisualisationStage&&) = delete;
    VisualisationStage& operator=(VisualisationStage&&) = delete;

    void render(const FrameContext& context) override;

    private:
    core::Logger logger{"VisualisationStage"};

    const vk::RenderContext& renderContext;
    vk::Device& device;

    // Overflow past MAX_BALLS / MAX_LINES is warned about once, not per frame.
    bool warnedBallOverflow = false;
    bool warnedLineOverflow = false;

    std::unique_ptr<vk::ShaderEffect> ballShaderEffect;
    std::unique_ptr<vk::ShaderEffect> lineShaderEffect;

    std::unique_ptr<vk::DescriptorSetLayout> primitiveSetLayout;
    std::unique_ptr<vk::Pipeline> ballPipeline;
    std::unique_ptr<vk::Pipeline> linePipeline;

    // One set of per-frame-in-flight resources, indexed by FrameContext::frameIndex.
    std::vector<std::unique_ptr<vk::Buffer>> sceneUbos;
    std::vector<std::unique_ptr<vk::DescriptorSet>> sceneSets;
    std::vector<std::unique_ptr<vk::Buffer>> ballSsbos;
    std::vector<std::unique_ptr<vk::Buffer>> lineSsbos;
    // Single descriptor set per frame binding both the Balls (binding 0) and
    // Lines (binding 1) storage buffers; both pipelines bind the same set
    // and each only references the one binding its own shaders use.
    std::vector<std::unique_ptr<vk::DescriptorSet>> primitiveSets;

    void createDescriptorSetLayout();
    void createFrameResources();
    void createPipelines();
};

} // namespace nacre::render
