#include "skin/SkinApp.hpp"

#include <algorithm>
#include <memory>

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

#include "skin/CameraController.hpp"
#include "skin/geometry/WeightedPoint.hpp"
#include "skin/vis/ComplexVisualiser.hpp"
#include <nacre/assets/MaterialManager.hpp>
#include <nacre/platform/GlfwWindow.hpp>
#include <nacre/platform/InputManager.hpp>
#include <nacre/render/RenderScene.hpp>

namespace skin {

SkinApp::SkinApp() : App({.title = "Physics Application"}) {
    bindPointInsertion();
    cameraController =
        std::make_unique<CameraController>(getInput(), getWindow(), getRenderScene().getCamera());

    visualiser = std::make_unique<vis::ComplexVisualiser>(getEcs(), getMeshManager(),
                                                          getMaterialManager(), model);

    loadWeightedPoints();
    produceTriangularisation();

    // Layers were built from the default settings by produceTriangularisation();
    // this only applies the initial wireframe state, as before.
    getMaterialManager().setWireframe(userSettings.wireframe);

    logger.print("Application started");
}

SkinApp::~SkinApp() = default;

void SkinApp::onUpdate(double /*dt*/) {
    // UI changes from last frame's ImGuiPass, applied before this frame renders.
    if (pendingSettings) {
        applyUserSettings(*pendingSettings);
        pendingSettings.reset();
    }
    if (debugPanel.takeCompareRequest()) {
        startComparison();
    }
    collectMeshingResults();
}

void SkinApp::collectMeshingResults() {
    if (auto build = skinTask.poll()) {
        model.setSkin(std::move(*build));
        visualiser->rebuildSkinLayer(userSettings);
    }
    if (auto comparison = compareTask.poll(); comparison && *comparison) {
        model.setComparison(**comparison);
    }
}

void SkinApp::applyUserSettings(const vis::UserSettings& settings) {
    const vis::UserSettings previous = userSettings;
    userSettings = settings;

    if (settings.inputFile != previous.inputFile) {
        loadWeightedPoints();
        produceTriangularisation(); // rebuilds every layer from userSettings
    } else {
        bool remixed = true;
        if (settings.skinBoxPadding != previous.skinBoxPadding ||
            settings.displayBoxPadding != previous.displayBoxPadding) {
            model.rebuildClipping(settings.skinBoxPadding, settings.displayBoxPadding,
                                  settings.mixedS);
        } else if (settings.mixedS != previous.mixedS) {
            model.rebuildMixed(settings.mixedS);
        } else {
            remixed = false;
        }
        if (remixed) {
            compareTask.cancel(); // its mixed complex is gone
        }
        if (remixed || settings.showSkin != previous.showSkin ||
            settings.skinMesher != previous.skinMesher ||
            settings.skinRefineK != previous.skinRefineK ||
            settings.skinCgalSubdivisions != previous.skinCgalSubdivisions) {
            rebuildSkin();
        }
        visualiser->applySettings(previous, settings);
    }
    if (settings.wireframe != previous.wireframe) {
        getMaterialManager().setWireframe(settings.wireframe);
    }
}

void SkinApp::loadWeightedPoints() {
    try {
        model.load(userSettings.inputFile);
    } catch (const std::exception& e) {
        logger.error("Failed to load weighted points: {}", e.what());
        return;
    }

    logger.print("Loaded {} weighted points", model.points().size());
}

void SkinApp::produceTriangularisation() {
    // selectedVertex indexes vertices(), whose order changes on every rebuild;
    // carry the selection over by input index.
    std::optional<uint32_t> selectedInput;
    const auto& before = model.complex().vertices();
    if (userSettings.selectedVertex >= 0 &&
        static_cast<size_t>(userSettings.selectedVertex) < before.size()) {
        selectedInput = before[static_cast<size_t>(userSettings.selectedVertex)].inputIndex;
    }

    model.rebuild(userSettings.skinBoxPadding, userSettings.displayBoxPadding,
                  userSettings.mixedS);
    compareTask.cancel();
    rebuildSkin();

    if (selectedInput) {
        const auto& after = model.complex().vertices();
        for (size_t i = 0; i < after.size(); ++i) {
            if (after[i].inputIndex == *selectedInput) {
                userSettings.selectedVertex = static_cast<int>(i);
                break;
            }
        }
    }

    visualiser->rebuildAll(userSettings);
}

void SkinApp::rebuildSkin() {
    if (!userSettings.showSkin) {
        skinTask.cancel();
        model.clearSkin();
        return;
    }
    // The old skin stays drawn until the new one arrives.
    const auto inputs = std::make_shared<const geometry::SkinInputs>(model.skinInputs());
    const geometry::SkinRequest request{.mesher = userSettings.skinMesher,
                                        .s = userSettings.mixedS,
                                        .k = userSettings.skinRefineK,
                                        .subdivisions = userSettings.skinCgalSubdivisions};
    skinTask.start([inputs, request](geometry::MeshingControl& control) {
        return geometry::buildSkin(*inputs, request, control);
    });
}

void SkinApp::startComparison() {
    const auto inputs = std::make_shared<const geometry::SkinInputs>(model.skinInputs());
    const geometry::SkinRequest request{.mesher = userSettings.skinMesher,
                                        .s = userSettings.mixedS,
                                        .k = userSettings.skinRefineK,
                                        .subdivisions = userSettings.skinCgalSubdivisions};
    compareTask.start([inputs, request](geometry::MeshingControl& control) {
        return geometry::compareSkinMeshes(*inputs, request, control);
    });
}

void SkinApp::onUi() {
    if (auto edited = debugPanel.draw(userSettings, model, visualiser->mixedCellCounts(),
                                      skinTask.status(), compareTask.status())) {
        pendingSettings = *edited;
    }
}

glm::vec2 SkinApp::cursorToNdc(double x, double y) const {
    int width, height;
    glfwGetWindowSize(getWindow().getNativeWindow(), &width, &height);
    // The projection flips Y for Vulkan, so NDC +Y points down -- same as
    // GLFW's top-left cursor origin. No Y inversion needed.
    return {2.0f * static_cast<float>(x) / static_cast<float>(std::max(width, 1)) - 1.0f,
            2.0f * static_cast<float>(y) / static_cast<float>(std::max(height, 1)) - 1.0f};
}

void SkinApp::bindPointInsertion() {
    pointInsertion = getInput().bindKey("P", [this](double /*dt*/) {
        // Cursor is disabled during right-drag look (its position is unbounded),
        // and keys typed into the panel belong to ImGui.
        if (glfwGetInputMode(getWindow().getNativeWindow(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED ||
            ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(getWindow().getNativeWindow(), &mouseX, &mouseY);

        // Place the point under the cursor on the camera-facing plane a fixed
        // distance ahead, so it lands exactly where you clicked from any view.
        constexpr float insertDistance = 5.0f;
        auto worldPos = getRenderScene().getCamera().unprojectToViewPlane(
            cursorToNdc(mouseX, mouseY), insertDistance);
        if (!worldPos) {
            logger.warn("Couldn't unproject mouse position onto the view plane");
            return;
        }

        glm::vec3 pos = worldPos.value();

        logger.print("Inserting point at ({}, {}, {})", pos.x, pos.y, pos.z);

        geometry::WeightedPoint weightedPoint(pos, 1.0f);
        model.addPoint(weightedPoint);
        produceTriangularisation(); // rebuilds every layer, the point markers included
    });
}

} // namespace skin
