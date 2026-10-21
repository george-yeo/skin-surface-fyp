#include "skin/vis/DebugPanel.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <vector>

#include "skin/geometry/BoundedVoronoi.hpp"
#include "skin/geometry/ComplexModel.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include "skin/geometry/SkinBuild.hpp"
#include "skin/geometry/SkinMesh.hpp"
#include "skin/vis/ComplexVisualiser.hpp"

namespace skin::vis {

namespace {

constexpr const char* INPUT_DIRECTORY = "assets/test";

// Sorted *.json paths under INPUT_DIRECTORY; empty if it can't be read.
std::vector<std::string> listInputFiles() {
    std::vector<std::string> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(INPUT_DIRECTORY, error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == ".json") {
            files.push_back(entry.path().generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void drawInputSelector(UserSettings& settings) {
    if (ImGui::BeginCombo("Input", settings.inputFile.c_str())) {
        for (const std::string& file : listInputFiles()) {
            if (ImGui::Selectable(file.c_str(), file == settings.inputFile)) {
                settings.inputFile = file;
            }
        }
        ImGui::EndCombo();
    }
}

void drawPowerComplexStats(const geometry::ComplexModel& model) {
    const geometry::PowerComplex& complex = model.complex();
    const geometry::Aabb& skinBox = model.skinVoronoi().box;
    const geometry::Aabb& displayBox = model.displayVoronoi().box;
    ImGui::Text("Dimension %d, %zu hidden inputs, %u ghosts", complex.dimension(),
                complex.hiddenInputs().size(), model.ghostCount());
    ImGui::Text("Delaunay: %zu V, %zu E, %zu F, %zu T", complex.vertices().size(),
                complex.edges().size(), complex.triangles().size(), complex.tets().size());

    const auto& edges = complex.voronoiEdges();
    const auto boundedEdges = std::count_if(edges.begin(), edges.end(), [](const auto& edge) {
        return edge.tets[1] != geometry::kInfinite;
    });
    const auto& cells = complex.voronoiCells();
    const auto boundedCells = std::count_if(cells.begin(), cells.end(),
                                            [](const auto& cell) { return cell.bounded; });
    ImGui::Text("Voronoi: %zu vertices", complex.voronoiVertices().size());
    ImGui::Text("  %zu / %zu edges bounded", static_cast<size_t>(boundedEdges), edges.size());
    ImGui::Text("  %zu / %zu cells bounded", static_cast<size_t>(boundedCells), cells.size());
    ImGui::Text("Skin box: (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)", skinBox.min.x,
                skinBox.min.y, skinBox.min.z, skinBox.max.x, skinBox.max.y, skinBox.max.z);
    ImGui::Text("Display box: (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)", displayBox.min.x,
                displayBox.min.y, displayBox.min.z, displayBox.max.x, displayBox.max.y,
                displayBox.max.z);
}

void drawSelectedCellControls(const geometry::PowerComplex& complex, UserSettings& settings) {
    const auto& vertices = complex.vertices();
    if (vertices.empty()) {
        ImGui::TextDisabled("No vertices");
        return;
    }

    const int maxIndex = static_cast<int>(vertices.size()) - 1;
    settings.selectedVertex = std::clamp(settings.selectedVertex, 0, maxIndex);
    ImGui::SliderInt("Vertex", &settings.selectedVertex, 0, maxIndex, "%d",
                     ImGuiSliderFlags_AlwaysClamp);

    const auto& vertex = vertices[static_cast<size_t>(settings.selectedVertex)];
    ImGui::Text("Input #%u, weight %.3f", vertex.inputIndex, vertex.weight);

    const auto& cells = complex.voronoiCells();
    if (static_cast<size_t>(settings.selectedVertex) < cells.size()) {
        const auto& cell = cells[static_cast<size_t>(settings.selectedVertex)];
        ImGui::Text("%zu faces, %s", cell.faces.size(),
                    cell.bounded ? "bounded" : "unbounded (clipped to box)");
    }
}

// A slider that edits `draft` freely but only writes `committed` when the
// user releases it (or finishes typing), so the expensive rebuild behind it
// runs once per edit instead of every frame of a drag.
void commitOnReleaseSlider(const char* label, float& draft, float& committed, float min,
                           float max) {
    // AlwaysClamp: Ctrl+click typing would otherwise bypass [min, max].
    ImGui::SliderFloat(label, &draft, min, max, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        committed = draft;
    } else if (!ImGui::IsItemActive()) {
        draft = committed; // pick up changes made elsewhere
    }
}

void drawMixedCellStats(const MixedCellCounts& counts) {
    ImGui::Text("Built: %u V, %u E, %u F, %u T", counts.built[0], counts.built[1],
                counts.built[2], counts.built[3]);
    ImGui::Text("Clipped by box: %u V, %u E, %u F, %u T", counts.clipped[0], counts.clipped[1],
                counts.clipped[2], counts.clipped[3]);
}

void drawSkinStats(const geometry::SkinBuild& skin) {
    const geometry::SkinMeshStats& stats = skin.stats;
    ImGui::Text("%zu V, %zu E, %zu F (%.1f ms)", stats.vertices, stats.edges, stats.triangles,
                skin.milliseconds);
    std::string chi;
    for (const int64_t value : stats.eulerCharacteristics) {
        chi += (chi.empty() ? "" : ", ") + std::to_string(value);
    }
    ImGui::Text("%zu components, chi: %s", stats.eulerCharacteristics.size(), chi.c_str());
    if (stats.nonManifoldEdges > 0) {
        ImGui::TextColored({1.0f, 0.4f, 0.3f, 1.0f}, "%zu non-manifold edges",
                           stats.nonManifoldEdges);
    } else if (stats.triangles > 0) {
        ImGui::Text("Watertight");
    }
    if (skin.mesher == geometry::SkinMesher::MarchingTets) {
        ImGui::Text("%zu tets%s", skin.tets, skin.budgetExceeded ? " (budget hit)" : "");
    }
    if (skin.mesher == geometry::SkinMesher::KruithofVegter && stats.triangles > 0) {
        if (skin.monotonicityViolations == 0) {
            ImGui::Text("%zu tets; Condition 17 holds (isotopic)", skin.tets);
        } else {
            ImGui::TextColored({1.0f, 0.4f, 0.3f, 1.0f}, "%zu tets; %zu Condition 17 violations",
                               skin.tets, skin.monotonicityViolations);
        }
    }
}

void drawSkinComparison(const geometry::SkinComparison& comparison) {
    ImGui::Text("Ours %zu V / %zu F, CGAL %zu V / %zu F", comparison.oursVertices,
                comparison.oursTriangles, comparison.cgalVertices, comparison.cgalTriangles);
    ImGui::Text("%zu / %zu CGAL vertices located", comparison.located,
                comparison.cgalVertices);
    ImGui::Text("|F| / |grad F|: max %.2e, mean %.2e", comparison.maxResidual,
                comparison.meanResidual);
    ImGui::Text("CGAL -> ours: max %.2e, mean %.2e", comparison.maxCgalToOurs,
                comparison.meanCgalToOurs);
    ImGui::Text("Ours -> CGAL: max %.2e, mean %.2e", comparison.maxOursToCgal,
                comparison.meanOursToCgal);
}

const char* phaseName(geometry::MeshingPhase phase) {
    switch (phase) {
    case geometry::MeshingPhase::Starting:
        return "Starting";
    case geometry::MeshingPhase::Refining:
        return "Refining";
    case geometry::MeshingPhase::Triangulating:
        return "Triangulating the mixed complex";
    case geometry::MeshingPhase::Marching:
        return "Marching";
    case geometry::MeshingPhase::Cgal:
        return "CGAL meshing";
    case geometry::MeshingPhase::Comparing:
        return "Comparing";
    case geometry::MeshingPhase::Statistics:
        return "Checking mesh";
    }
    return "";
}

// A line of text and a bar; indeterminate while the phase's total is unknown.
void drawMeshingStatus(const geometry::MeshingStatus& status) {
    std::string text = std::format("{}... {:.1f} s", phaseName(status.phase), status.seconds);
    if (status.tets > 0) {
        text += std::format(", {}k tets", status.tets / 1000);
    }
    ImGui::TextUnformatted(text.c_str());
    // ImGui animates a negative fraction as an indeterminate bar; it must
    // change with time.
    const bool known = status.fraction >= 0.0f;
    ImGui::ProgressBar(known ? status.fraction : -static_cast<float>(ImGui::GetTime()),
                       ImVec2(-FLT_MIN, 0.0f), known ? nullptr : "");
}

} // namespace

std::optional<UserSettings> DebugPanel::draw(const UserSettings& current,
                                             const geometry::ComplexModel& model,
                                             const MixedCellCounts& mixedCounts,
                                             const geometry::MeshingStatus& skinStatus,
                                             const geometry::MeshingStatus& compareStatus) {
    UserSettings settings = current;
    ImGui::Begin("Settings");

    // ImGui's own rolling average over the last ~60 frames.
    const float framerate = ImGui::GetIO().Framerate;
    ImGui::Text("%.1f FPS (%.2f ms)", framerate, framerate > 0.0f ? 1000.0f / framerate : 0.0f);

    drawInputSelector(settings);

    ImGui::SeparatorText("Layers");
    ImGui::Checkbox("Points", &settings.showPoints);
    ImGui::SameLine();
    ImGui::Checkbox("Weights", &settings.showWeights);
    ImGui::Checkbox("Delaunay faces", &settings.showDelaunayFaces);
    if (settings.showDelaunayFaces) {
        commitOnReleaseSlider("Alpha##faces", delaunayFaceAlphaDraft, settings.delaunayFaceAlpha,
                              0.05f, 1.0f);
    }
    ImGui::Checkbox("Delaunay edges", &settings.showDelaunayEdges);
    ImGui::Checkbox("Voronoi edges", &settings.showVoronoiEdges);
    ImGui::Checkbox("Delaunay tets (shrunk)", &settings.showDelaunayTets);
    if (settings.showDelaunayTets) {
        commitOnReleaseSlider("Tet shrink", tetShrinkDraft, settings.tetShrink, 0.1f, 1.0f);
        commitOnReleaseSlider("Alpha##tets", tetAlphaDraft, settings.tetAlpha, 0.05f, 1.0f);
    }
    ImGui::Checkbox("Voronoi cells (exploded)", &settings.showVoronoiCells);
    if (settings.showVoronoiCells) {
        commitOnReleaseSlider("Cell shrink", cellShrinkDraft, settings.cellShrink, 0.1f, 1.0f);
        commitOnReleaseSlider("Alpha##cells", cellAlphaDraft, settings.cellAlpha, 0.05f, 1.0f);
    }
    ImGui::Checkbox("Mixed cells", &settings.showMixedCells);
    if (settings.showMixedCells) {
        commitOnReleaseSlider("s", mixedSDraft, settings.mixedS, 0.01f, 0.99f);
        commitOnReleaseSlider("Gap", mixedGapDraft, settings.mixedGap, 0.1f, 1.0f);
        commitOnReleaseSlider("Alpha##mixed", mixedAlphaDraft, settings.mixedAlpha, 0.05f, 1.0f);
        ImGui::Checkbox("Vertex cells", &settings.showMixedVertexCells);
        ImGui::SameLine();
        ImGui::Checkbox("Edge cells", &settings.showMixedEdgeCells);
        ImGui::Checkbox("Triangle cells", &settings.showMixedTriangleCells);
        ImGui::SameLine();
        ImGui::Checkbox("Tet cells", &settings.showMixedTetCells);
        drawMixedCellStats(mixedCounts);
    }
    ImGui::Checkbox("Selected cell", &settings.showSelectedCell);
    if (settings.showSelectedCell) {
        drawSelectedCellControls(model.complex(), settings);
        commitOnReleaseSlider("Alpha##selected", selectedCellAlphaDraft,
                              settings.selectedCellAlpha, 0.05f, 1.0f);
    }
    ImGui::Checkbox("Skin", &settings.showSkin);
    if (settings.showSkin) {
        // Same order as geometry::SkinMesher.
        const char* meshers[] = {"Generic marching tets", "CGAL", "Kruithof-Vegter"};
        int mesher = static_cast<int>(settings.skinMesher);
        if (ImGui::Combo("Mesher", &mesher, meshers, IM_ARRAYSIZE(meshers))) {
            settings.skinMesher = static_cast<geometry::SkinMesher>(mesher);
        }
        commitOnReleaseSlider("s##skin", skinSDraft, settings.mixedS, 0.01f, 0.99f);
        if (settings.skinMesher == geometry::SkinMesher::MarchingTets) {
            commitOnReleaseSlider("k (edge / patch radius)", skinRefineKDraft,
                                  settings.skinRefineK, 0.25f, 2.0f);
        }
        if (settings.skinMesher == geometry::SkinMesher::Cgal) {
            ImGui::SliderInt("Subdivisions", &settings.skinCgalSubdivisions, 0, 3, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
        } else {
            ImGui::Checkbox("Colour by cell type", &settings.skinColorByCell);
        }
        if (skinStatus.busy) {
            drawMeshingStatus(skinStatus);
            if (!model.skin().mesh.empty()) {
                ImGui::TextDisabled("Showing the previous mesh");
            }
        }
        drawSkinStats(model.skin());

        ImGui::BeginDisabled(compareStatus.busy);
        if (ImGui::Button("Compare with CGAL")) {
            compareRequested = true;
        }
        ImGui::EndDisabled();
        if (compareStatus.busy) {
            drawMeshingStatus(compareStatus);
        } else if (const auto& comparison = model.skinComparison()) {
            drawSkinComparison(*comparison);
        }
    }
    ImGui::Checkbox("Clip box", &settings.showClipBox);
    commitOnReleaseSlider("Skin box padding", skinBoxPaddingDraft, settings.skinBoxPadding, 0.0f,
                          2.0f);
    commitOnReleaseSlider("Display box padding", displayBoxPaddingDraft,
                          settings.displayBoxPadding, 0.0f, 3.0f);

    ImGui::SeparatorText("Power complex");
    drawPowerComplexStats(model);

    ImGui::End();

    if (settings != current) {
        return settings;
    }
    return std::nullopt;
}

} // namespace skin::vis
