#include <nacre/render/ImGuiPass.hpp>

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

#include <cmath>
#include <utility>

#include <nacre/platform/GlfwWindow.hpp>
#include <nacre/vk/DescriptorPool.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/vk/Swapchain.hpp>
#include <nacre/vk/Instance.hpp>

namespace nacre::render {

namespace {

float srgbToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace

ImGuiPass::ImGuiPass(platform::GlfwWindow& window, const vk::RenderContext& renderContext,
                     std::function<void()> drawUi)
    : drawUi(std::move(drawUi)) {
    vk::Device& device = renderContext.device;
    vk::DescriptorPool& descriptorPool = renderContext.descriptorPool;
    const VkFormat colorFormat = renderContext.swapchain().getFormat();
    const auto imageCount = static_cast<uint32_t>(renderContext.swapchain().getImageCount());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // ImGui's colours are authored in sRGB. On an sRGB swapchain the hardware
    // encodes on write, so decode them once or the UI comes out washed out.
    if (colorFormat == VK_FORMAT_B8G8R8A8_SRGB || colorFormat == VK_FORMAT_R8G8B8A8_SRGB) {
        for (ImVec4& color : ImGui::GetStyle().Colors) {
            color.x = srgbToLinear(color.x);
            color.y = srgbToLinear(color.y);
            color.z = srgbToLinear(color.z);
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForVulkan(window.getNativeWindow(), true);

    VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = device.getInstance().get();
    info.PhysicalDevice = device.getPhysicalDevice();
    info.Device = device.get();
    info.QueueFamily = device.getGraphicsFamilyIndex();
    info.Queue = device.getGraphicsQueue();
    info.DescriptorPool = descriptorPool.get();
    info.MinImageCount = imageCount;
    info.ImageCount = imageCount;
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;
    ImGui_ImplVulkan_Init(&info);
}

ImGuiPass::~ImGuiPass() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiPass::render(const FrameContext& context) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (drawUi) {
        drawUi();
    }

    ImGui::Render();

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = context.colorView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, context.extent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(context.cmd, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), context.cmd);
    vkCmdEndRendering(context.cmd);
}

} // namespace nacre::render
