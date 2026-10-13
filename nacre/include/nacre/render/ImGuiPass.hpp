#pragma once

#include <functional>

#include <nacre/platform/fwd.hpp>
#include <nacre/render/FrameStage.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::render {

class ImGuiPass : public FrameStage {
    public:
    ImGuiPass(platform::GlfwWindow& window, const vk::RenderContext& renderContext,
              std::function<void()> drawUi);
    ~ImGuiPass() override;

    ImGuiPass(const ImGuiPass&) = delete;
    ImGuiPass& operator=(const ImGuiPass&) = delete;

    ImGuiPass(ImGuiPass&&) = delete;
    ImGuiPass& operator=(ImGuiPass&&) = delete;

    void render(const FrameContext& context) override;

    private:
    std::function<void()> drawUi;
};

} // namespace nacre::render
