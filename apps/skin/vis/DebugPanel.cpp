#include "skin/vis/DebugPanel.hpp"

#include <imgui/imgui.h>

#include <algorithm>

#include "skin/geometry/BoundedVoronoi.hpp"
#include "skin/geometry/ComplexModel.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include "skin/vis/ComplexVisualiser.hpp"

namespace skin::vis {

namespace {

void drawPowerComplexStats(const geometry::PowerComplex& complex, const geometry::Aabb& skinBox,
                           const geometry::Aabb& displayBox) {
    ImGui::Text("Dimension %d, %zu hidden inputs", complex.dimension(),
                complex.hiddenInputs().size());
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

} // namespace

std::optional<UserSettings> DebugPanel::draw(const UserSettings& current,
                                             const geometry::ComplexModel& model,
                                             const MixedCellCounts& mixedCounts) {
    UserSettings settings = current;
    ImGui::Begin("Settings");

    // ImGui's own rolling average over the last ~60 frames.
    const float framerate = ImGui::GetIO().Framerate;
    ImGui::Text("%.1f FPS (%.2f ms)", framerate, framerate > 0.0f ? 1000.0f / framerate : 0.0f);

    ImGui::SeparatorText("Layers");
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
    ImGui::Checkbox("Clip box", &settings.showClipBox);
    commitOnReleaseSlider("Skin box padding", skinBoxPaddingDraft, settings.skinBoxPadding, 0.0f,
                          2.0f);
    commitOnReleaseSlider("Display box padding", displayBoxPaddingDraft,
                          settings.displayBoxPadding, 0.0f, 3.0f);

    ImGui::SeparatorText("Power complex");
    drawPowerComplexStats(model.complex(), model.skinVoronoi().box,
                          model.displayVoronoi().box);

    ImGui::End();

    if (settings != current) {
        return settings;
    }
    return std::nullopt;
}

} // namespace skin::vis
