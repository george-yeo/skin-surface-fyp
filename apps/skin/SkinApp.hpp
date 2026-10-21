#pragma once

#include <memory>
#include <optional>

#include <glm/glm.hpp>

#include <nacre/app/App.hpp>
#include <nacre/core/Logger.hpp>
#include <nacre/platform/InputBinding.hpp>
#include "skin/BackgroundTask.hpp"
#include "skin/geometry/ComplexModel.hpp"
#include "skin/vis/DebugPanel.hpp"
#include "skin/vis/UserSettings.hpp"
#include "skin/fwd.hpp"
#include "skin/vis/fwd.hpp"

namespace skin {

class SkinApp : public nacre::app::App {
    public:
    SkinApp();
    ~SkinApp() override;

    protected:
    void onUpdate(double dt) override;
    void onUi() override;

    private:
    nacre::core::Logger logger{"SkinApp"};

    vis::UserSettings userSettings;
    // Applied at the start of the next update(), never mid-frame.
    std::optional<vis::UserSettings> pendingSettings;

    geometry::ComplexModel model;

    std::unique_ptr<CameraController> cameraController;
    std::unique_ptr<vis::ComplexVisualiser> visualiser;
    vis::DebugPanel debugPanel;

    // Copy the inputs they need, so they don't reference the model.
    BackgroundTask<geometry::SkinBuild> skinTask;
    BackgroundTask<std::optional<geometry::SkinComparison>> compareTask;

    // Declared last: unbinds before the members its callback uses are destroyed.
    nacre::platform::InputBinding pointInsertion;

    glm::vec2 cursorToNdc(double x, double y) const;

    void applyUserSettings(const vis::UserSettings& settings);
    void loadWeightedPoints();
    void produceTriangularisation();
    void rebuildSkin(); // starts a job, or clears the skin when hidden
    void startComparison();
    void collectMeshingResults();

    void bindPointInsertion();
};

} // namespace skin
