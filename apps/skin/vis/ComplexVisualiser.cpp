#include "skin/vis/ComplexVisualiser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <utility>

#include <nacre/scene/ColorComponent.hpp>
#include <nacre/ecs/ECS.hpp>
#include "skin/geometry/BoundedVoronoi.hpp"
#include "skin/geometry/ComplexModel.hpp"
#include "skin/geometry/MixedComplex.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include "skin/geometry/SkinMesh.hpp"
#include <nacre/assets/MaterialManager.hpp>
#include <nacre/assets/MeshManager.hpp>
#include "skin/geometry/WeightedPoint.hpp"
#include "skin/vis/Spawn.hpp"

namespace skin::vis {

namespace {

constexpr float EDGE_THICKNESS = 0.02f;
constexpr float POINT_MARKER_RADIUS = 0.1f;
constexpr float HIGHLIGHT_THICKNESS = 0.04f;
constexpr double MIN_EDGE_LENGTH = 1e-6; // skip coincident Voronoi vertices

const glm::vec4 WHITE{1.0f, 1.0f, 1.0f, 1.0f};
const glm::vec4 VORONOI_COLOR{0.25f, 0.9f, 0.45f, 1.0f};
const glm::vec4 CLIP_BOX_COLOR{0.6f, 0.6f, 0.75f, 1.0f};
const glm::vec4 DISPLAY_BOX_COLOR{0.35f, 0.35f, 0.45f, 1.0f};
const glm::vec3 SKIN_COLOR{0.85f, 0.8f, 0.75f};
// By Delaunay simplex dimension, for mixed cells and the skin patches in them.
const std::array<glm::vec3, 4> CELL_COLORS = {glm::vec3(0.35f, 0.55f, 0.95f),  // vertex
                                              glm::vec3(0.35f, 0.8f, 0.45f),   // edge
                                              glm::vec3(0.95f, 0.75f, 0.3f),   // triangle
                                              glm::vec3(0.9f, 0.35f, 0.35f)};  // tet

using PointKey = std::array<double, 3>;

glm::vec3 toFloat(const glm::dvec3& v) {
    return glm::vec3(v);
}

glm::dvec3 shrinkToward(const glm::dvec3& point, const glm::dvec3& center, double factor) {
    return center + (point - center) * factor;
}

// Flat convex polygon as a triangle fan; vertices duplicated per polygon so
// each carries its own face normal, oriented away from `interior`.
void appendPolygon(nacre::assets::MeshData& mesh, const std::vector<glm::dvec3>& polygon,
                   const glm::dvec3& interior, const glm::vec3& color) {
    if (polygon.size() < 3) {
        return;
    }

    // Newell's method: sum of cross products = 2 * area * normal.
    glm::dvec3 normal(0.0);
    for (size_t k = 0; k < polygon.size(); ++k) {
        normal += glm::cross(polygon[k], polygon[(k + 1) % polygon.size()]);
    }
    const double length = glm::length(normal);
    if (length < 1e-12) {
        return;
    }
    normal /= length;
    // The ring is counter-clockwise about the Newell normal; keep triangles
    // counter-clockwise about the outward one so back faces can be culled.
    const bool flipped = glm::dot(normal, polygon[0] - interior) < 0.0;
    if (flipped) {
        normal = -normal;
    }

    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (const glm::dvec3& point : polygon) {
        mesh.vertices.push_back(
            nacre::assets::Vertex{.position = toFloat(point), .normal = toFloat(normal), .color = color});
    }
    for (uint32_t k = 1; k + 1 < polygon.size(); ++k) {
        if (flipped) {
            mesh.indices.insert(mesh.indices.end(), {base, base + k + 1, base + k});
        } else {
            mesh.indices.insert(mesh.indices.end(), {base, base + k, base + k + 1});
        }
    }
}

// Both windings with opposite normals, for walls seen from either side.
void appendTwoSided(nacre::assets::MeshData& mesh, const std::vector<glm::dvec3>& polygon,
                    const glm::vec3& color) {
    if (polygon.size() < 3) {
        return;
    }
    glm::dvec3 normal(0.0);
    for (size_t k = 0; k < polygon.size(); ++k) {
        normal += glm::cross(polygon[k], polygon[(k + 1) % polygon.size()]);
    }
    const double length = glm::length(normal);
    if (length < 1e-12) {
        return;
    }
    normal /= length;

    for (const double side : {1.0, -1.0}) {
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (const glm::dvec3& point : polygon) {
            mesh.vertices.push_back(nacre::assets::Vertex{
                .position = toFloat(point), .normal = toFloat(side * normal), .color = color});
        }
        for (uint32_t k = 1; k + 1 < polygon.size(); ++k) {
            if (side > 0.0) {
                mesh.indices.insert(mesh.indices.end(), {base, base + k, base + k + 1});
            } else {
                mesh.indices.insert(mesh.indices.end(), {base, base + k + 1, base + k});
            }
        }
    }
}

glm::vec4 withAlpha(const glm::vec4& color, float alpha) {
    return {color.r, color.g, color.b, alpha};
}

// Golden-ratio hue steps, fixed saturation/value (HSV -> RGB).
glm::vec3 indexColor(uint32_t index) {
    const float hue = std::fmod(static_cast<float>(index) * 0.618034f, 1.0f);
    const glm::vec3 k =
        glm::abs(glm::fract(glm::vec3(hue) + glm::vec3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f -
                 3.0f);
    return 0.9f * glm::mix(glm::vec3(1.0f), glm::clamp(k - 1.0f, 0.0f, 1.0f), 0.55f);
}

} // namespace

ComplexVisualiser::ComplexVisualiser(nacre::ecs::ECS& ecs, nacre::assets::MeshManager& meshManager,
                                     nacre::assets::MaterialManager& materialManager,
                                     const geometry::ComplexModel& model)
    : ecs(ecs),
      meshManager(meshManager),
      materialManager(materialManager),
      complexModel(model),
      complex(model.complex()),
      skinVoronoi(model.skinVoronoi()),
      displayVoronoi(model.displayVoronoi()),
      points(model.points()) {}

void ComplexVisualiser::rebuildAll(const UserSettings& settings) {
    rebuildPointLayer(settings);
    rebuildWeightLayer(settings);
    rebuildDelaunayFaceLayer(settings);
    rebuildDelaunayEdgeLayer(settings);
    rebuildVoronoiEdgeLayer(settings);
    rebuildDelaunayTetLayer(settings);
    rebuildVoronoiCellLayer(settings);
    rebuildSelectedCellLayer(settings);
    rebuildMixedCellLayer(settings);
    rebuildClipBoxLayer(settings);
    rebuildSkinLayer(settings);
}

void ComplexVisualiser::applySettings(const UserSettings& previous, const UserSettings& next) {
    // New paddings mean the owner has rebuilt both clipped diagrams.
    const bool reclipped = next.skinBoxPadding != previous.skinBoxPadding ||
                           next.displayBoxPadding != previous.displayBoxPadding;
    if (next.showPoints != previous.showPoints) {
        rebuildPointLayer(next);
    }
    if (next.showWeights != previous.showWeights) {
        rebuildWeightLayer(next);
    }
    if (next.showDelaunayFaces != previous.showDelaunayFaces ||
        next.delaunayFaceAlpha != previous.delaunayFaceAlpha) {
        rebuildDelaunayFaceLayer(next);
    }
    if (next.showDelaunayEdges != previous.showDelaunayEdges) {
        rebuildDelaunayEdgeLayer(next);
    }
    if (reclipped || next.showVoronoiEdges != previous.showVoronoiEdges) {
        rebuildVoronoiEdgeLayer(next);
    }
    if (next.showDelaunayTets != previous.showDelaunayTets ||
        next.tetShrink != previous.tetShrink || next.tetAlpha != previous.tetAlpha) {
        rebuildDelaunayTetLayer(next);
    }
    if (reclipped || next.showVoronoiCells != previous.showVoronoiCells ||
        next.cellShrink != previous.cellShrink || next.cellAlpha != previous.cellAlpha) {
        rebuildVoronoiCellLayer(next);
    }
    if (reclipped || next.showSelectedCell != previous.showSelectedCell ||
        next.selectedVertex != previous.selectedVertex ||
        next.selectedCellAlpha != previous.selectedCellAlpha) {
        rebuildSelectedCellLayer(next);
    }
    if (reclipped || next.showMixedCells != previous.showMixedCells ||
        next.mixedS != previous.mixedS ||
        next.mixedGap != previous.mixedGap || next.mixedAlpha != previous.mixedAlpha ||
        next.showMixedVertexCells != previous.showMixedVertexCells ||
        next.showMixedEdgeCells != previous.showMixedEdgeCells ||
        next.showMixedTriangleCells != previous.showMixedTriangleCells ||
        next.showMixedTetCells != previous.showMixedTetCells) {
        rebuildMixedCellLayer(next);
    }
    if (reclipped || next.showClipBox != previous.showClipBox) {
        rebuildClipBoxLayer(next);
    }
    // A new skin mesh arrives later, through rebuildSkinLayer.
    if (next.showSkin != previous.showSkin || next.skinColorByCell != previous.skinColorByCell) {
        rebuildSkinLayer(next);
    }
}

template <size_t N>
bool ComplexVisualiser::touchesGhost(const std::array<uint32_t, N>& simplex) const {
    return std::any_of(simplex.begin(), simplex.end(),
                       [&](uint32_t v) { return complexModel.isGhost(v); });
}

void ComplexVisualiser::addLineEntity(EntityLayer& layer, const glm::vec3& start,
                                      const glm::vec3& end, float thickness,
                                      const glm::vec4& color) {
    nacre::ecs::Entity entity = spawnLine(ecs, start, end, thickness);
    nacre::scene::ColorComponent colorComponent{.color = color};
    ecs.addComponent(entity, colorComponent);
    layer.add(entity);
}

void ComplexVisualiser::addMeshEntity(EntityLayer& layer, const std::string& name,
                                      const nacre::assets::MeshData& mesh, const glm::vec4& color) {
    meshManager.addMesh(name, mesh);
    layer.addMesh(name);

    nacre::ecs::Entity entity = spawnMesh(ecs, glm::vec3(0.0f, 0.0f, 0.0f),
                                          meshManager.getHandle(name).value(),
                                          materialManager.getHandle("test").value());
    nacre::scene::ColorComponent colorComponent{.color = color};
    ecs.addComponent(entity, colorComponent);
    layer.add(entity);
}

void ComplexVisualiser::rebuildPointLayer(const UserSettings& settings) {
    pointLayer.destroyAll(ecs, meshManager);
    if (!settings.showPoints) {
        return;
    }
    for (const auto& point : points) {
        pointLayer.add(spawnPoint(ecs, point.position, POINT_MARKER_RADIUS));
    }
}

void ComplexVisualiser::rebuildWeightLayer(const UserSettings& settings) {
    weightLayer.destroyAll(ecs, meshManager);
    if (!settings.showWeights) {
        return;
    }

    // Weight is r^2.
    for (const auto& point : points) {
        const float radius = static_cast<float>(std::sqrt(std::max(point.weight, 0.0)));
        weightLayer.add(spawnPoint(ecs, point.position, radius));
    }
}

void ComplexVisualiser::rebuildDelaunayFaceLayer(const UserSettings& settings) {
    delaunayFaceLayer.destroyAll(ecs, meshManager);
    if (!settings.showDelaunayFaces || complex.triangles().empty()) {
        return;
    }

    // Internal walls, visible from both sides.
    const auto& vertices = complex.vertices();
    nacre::assets::MeshData mesh{};
    std::vector<glm::dvec3> corners(3);
    for (const auto& triangle : complex.triangles()) {
        if (touchesGhost(triangle.v)) {
            continue;
        }
        for (size_t k = 0; k < 3; ++k) {
            corners[k] = vertices[triangle.v[k]].position;
        }
        appendTwoSided(mesh, corners, glm::vec3(1.0f));
    }

    if (!mesh.indices.empty()) {
        addMeshEntity(delaunayFaceLayer, "triangularisation", mesh,
                      {0.8f, 0.8f, 0.8f, settings.delaunayFaceAlpha});
    }
}

void ComplexVisualiser::rebuildDelaunayEdgeLayer(const UserSettings& settings) {
    delaunayEdgeLayer.destroyAll(ecs, meshManager);
    if (!settings.showDelaunayEdges) {
        return;
    }

    // No ColorComponent: default line gray.
    const auto& vertices = complex.vertices();
    for (const auto& edge : complex.edges()) {
        if (touchesGhost(edge.v)) {
            continue;
        }
        delaunayEdgeLayer.add(spawnLine(ecs, toFloat(vertices[edge.v[0]].position),
                                        toFloat(vertices[edge.v[1]].position), EDGE_THICKNESS));
    }
}

void ComplexVisualiser::rebuildVoronoiEdgeLayer(const UserSettings& settings) {
    voronoiEdgeLayer.destroyAll(ecs, meshManager);
    if (!settings.showVoronoiEdges) {
        return;
    }

    // Rays and segments already clipped to the box.
    for (const auto& segment : displayVoronoi.edges) {
        if (!segment || glm::length((*segment)[1] - (*segment)[0]) < MIN_EDGE_LENGTH) {
            continue;
        }
        addLineEntity(voronoiEdgeLayer, toFloat((*segment)[0]), toFloat((*segment)[1]),
                      EDGE_THICKNESS, VORONOI_COLOR);
    }
}

void ComplexVisualiser::rebuildDelaunayTetLayer(const UserSettings& settings) {
    delaunayTetLayer.destroyAll(ecs, meshManager);
    if (!settings.showDelaunayTets || complex.tets().empty()) {
        return;
    }

    // All tets in one mesh, colour per tet in the vertices.
    const auto& vertices = complex.vertices();
    const auto& tets = complex.tets();
    nacre::assets::MeshData mesh{};
    std::vector<glm::dvec3> triangle(3);
    for (uint32_t t = 0; t < tets.size(); ++t) {
        if (touchesGhost(tets[t].v)) {
            continue;
        }
        std::array<glm::dvec3, 4> p;
        for (size_t i = 0; i < 4; ++i) {
            p[i] = vertices[tets[t].v[i]].position;
        }
        const glm::dvec3 centroid = (p[0] + p[1] + p[2] + p[3]) * 0.25;
        const glm::vec3 color = indexColor(t);

        for (size_t face = 0; face < 4; ++face) { // face opposite vertex `face`
            for (size_t k = 0; k < 3; ++k) {
                triangle[k] = shrinkToward(p[(face + 1 + k) % 4], centroid, settings.tetShrink);
            }
            appendPolygon(mesh, triangle, centroid, color);
        }
    }

    if (!mesh.indices.empty()) {
        addMeshEntity(delaunayTetLayer, "delaunayTets", mesh, withAlpha(WHITE, settings.tetAlpha));
    }
}

void ComplexVisualiser::rebuildVoronoiCellLayer(const UserSettings& settings) {
    voronoiCellLayer.destroyAll(ecs, meshManager);
    if (!settings.showVoronoiCells) {
        return;
    }

    // All cells (clipped to the box) in one mesh, colour per cell in the
    // vertices.
    nacre::assets::MeshData mesh{};
    std::vector<glm::dvec3> polygon;
    for (uint32_t i = 0; i < displayVoronoi.cells.size(); ++i) {
        if (complexModel.isGhost(i)) {
            continue;
        }
        const geometry::ConvexPolytope& cell = displayVoronoi.cells[i];
        const glm::dvec3 centroid = cell.vertexMean();
        const glm::vec3 color = indexColor(i);
        for (const auto& face : cell.faces) {
            polygon.clear();
            for (const glm::dvec3& point : face.ring) {
                polygon.push_back(shrinkToward(point, centroid, settings.cellShrink));
            }
            appendPolygon(mesh, polygon, centroid, color);
        }
    }

    if (!mesh.indices.empty()) {
        addMeshEntity(voronoiCellLayer, "voronoiCells", mesh,
                      withAlpha(WHITE, settings.cellAlpha));
    }
}

void ComplexVisualiser::rebuildSelectedCellLayer(const UserSettings& settings) {
    selectedCellLayer.destroyAll(ecs, meshManager);
    const auto& vertices = complex.vertices();
    if (!settings.showSelectedCell || vertices.empty()) {
        return;
    }

    const uint32_t selected = static_cast<uint32_t>(
        std::clamp(settings.selectedVertex, 0, static_cast<int>(vertices.size()) - 1));

    // Delaunay edges at the vertex, each dual to one face of its cell. Scanned
    // directly so this also works below 3D.
    const glm::vec4 delaunayHighlight{0.4f, 0.85f, 1.0f, 1.0f};
    for (const auto& edge : complex.edges()) {
        if ((edge.v[0] == selected || edge.v[1] == selected) && !touchesGhost(edge.v)) {
            addLineEntity(selectedCellLayer, toFloat(vertices[edge.v[0]].position),
                          toFloat(vertices[edge.v[1]].position), HIGHLIGHT_THICKNESS,
                          delaunayHighlight);
        }
    }

    if (selected >= displayVoronoi.cells.size() || displayVoronoi.cells[selected].empty()) {
        return;
    }
    const geometry::ConvexPolytope& cell = displayVoronoi.cells[selected];

    const glm::dvec3 centroid = cell.vertexMean();
    nacre::assets::MeshData mesh{};
    for (const auto& face : cell.faces) {
        appendPolygon(mesh, face.ring, centroid, glm::vec3(1.0f));
    }
    if (!mesh.indices.empty()) {
        addMeshEntity(selectedCellLayer, "selectedCell", mesh,
                      {1.0f, 0.75f, 0.2f, settings.selectedCellAlpha});
    }

    // Its edges; each borders two faces and the clipper gives both the same
    // endpoints, so dedupe on exact coordinates.
    const glm::vec4 voronoiHighlight{1.0f, 1.0f, 0.3f, 1.0f};
    std::set<std::pair<PointKey, PointKey>> drawn;
    for (const auto& face : cell.faces) {
        const auto& ring = face.ring;
        for (size_t k = 0; k < ring.size(); ++k) {
            const glm::dvec3& a = ring[k];
            const glm::dvec3& b = ring[(k + 1) % ring.size()];
            PointKey keyA{a.x, a.y, a.z};
            PointKey keyB{b.x, b.y, b.z};
            if (keyB < keyA) {
                std::swap(keyA, keyB);
            }
            if (!drawn.insert({keyA, keyB}).second) {
                continue;
            }
            if (glm::length(b - a) >= MIN_EDGE_LENGTH) {
                addLineEntity(selectedCellLayer, toFloat(a), toFloat(b), HIGHLIGHT_THICKNESS,
                              voronoiHighlight);
            }
        }
    }
}

void ComplexVisualiser::rebuildMixedCellLayer(const UserSettings& settings) {
    mixedCellLayer.destroyAll(ecs, meshManager);
    mixedCounts = {};
    if (!settings.showMixedCells) {
        return;
    }

    const geometry::MixedComplex& mixed = complexModel.mixedComplex();
    mixedCounts.clipped = mixed.clipped;

    const std::array<bool, 4> enabled = {settings.showMixedVertexCells, settings.showMixedEdgeCells,
                                         settings.showMixedTriangleCells,
                                         settings.showMixedTetCells};
    nacre::assets::MeshData mesh{};
    std::vector<glm::dvec3> polygon;
    for (const geometry::MixedCell& cell : mixed.cells) {
        ++mixedCounts.built[cell.dimension];
        if (!enabled[cell.dimension]) {
            continue;
        }

        // Approximate centre (vertex average): only used for the visual gap and
        // to orient face normals outward, both fine for convex cells.
        glm::dvec3 centre(0.0);
        size_t count = 0;
        for (const auto& face : cell.faces) {
            for (const glm::dvec3& point : face) {
                centre += point;
                ++count;
            }
        }
        if (count == 0) {
            continue;
        }
        centre /= static_cast<double>(count);

        for (const auto& face : cell.faces) {
            polygon.clear();
            for (const glm::dvec3& point : face) {
                polygon.push_back(shrinkToward(point, centre, settings.mixedGap));
            }
            appendPolygon(mesh, polygon, centre, CELL_COLORS[cell.dimension]);
        }
    }

    if (!mesh.indices.empty()) {
        addMeshEntity(mixedCellLayer, "mixedCells", mesh, withAlpha(WHITE, settings.mixedAlpha));
    }
}

void ComplexVisualiser::rebuildClipBoxLayer(const UserSettings& settings) {
    clipBoxLayer.destroyAll(ecs, meshManager);
    if (!settings.showClipBox || complex.tets().empty()) {
        return;
    }

    // Corner k takes max on axis a when bit a of k is set; edges join corners
    // one bit apart.
    const auto addBox = [&](const geometry::Aabb& box, const glm::vec4& color) {
        const glm::vec3 lo = toFloat(box.min);
        const glm::vec3 hi = toFloat(box.max);
        const auto corner = [&](int k) {
            return glm::vec3((k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y,
                             (k & 4) ? hi.z : lo.z);
        };
        for (int k = 0; k < 8; ++k) {
            for (int bit = 1; bit < 8; bit <<= 1) {
                if ((k & bit) == 0) {
                    addLineEntity(clipBoxLayer, corner(k), corner(k | bit), EDGE_THICKNESS,
                                  color);
                }
            }
        }
    };
    addBox(skinVoronoi.box, CLIP_BOX_COLOR);
    addBox(displayVoronoi.box, DISPLAY_BOX_COLOR);
}

void ComplexVisualiser::rebuildSkinLayer(const UserSettings& settings) {
    skinLayer.destroyAll(ecs, meshManager);
    const geometry::SkinMesh& skin = complexModel.skin().mesh;
    if (!settings.showSkin || skin.empty()) {
        return;
    }

    // Coloured by cell: vertices duplicated per triangle so patches keep
    // sharp borders. Normals stay the smooth grad F ones either way.
    nacre::assets::MeshData mesh{};
    const auto vertex = [&](uint32_t v, const glm::vec3& color) {
        return nacre::assets::Vertex{.position = toFloat(skin.positions[v]),
                                     .normal = toFloat(skin.normals[v]),
                                     .color = color};
    };
    if (settings.skinColorByCell) {
        for (size_t t = 0; t < skin.triangles.size(); ++t) {
            const uint8_t dimension = skin.triangleDimension[t];
            const glm::vec3 color = dimension < 4 ? CELL_COLORS[dimension] : SKIN_COLOR;
            for (uint32_t v : skin.triangles[t]) {
                mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size()));
                mesh.vertices.push_back(vertex(v, color));
            }
        }
    } else {
        for (uint32_t v = 0; v < skin.positions.size(); ++v) {
            mesh.vertices.push_back(vertex(v, SKIN_COLOR));
        }
        for (const auto& triangle : skin.triangles) {
            mesh.indices.insert(mesh.indices.end(), triangle.begin(), triangle.end());
        }
    }
    addMeshEntity(skinLayer, "skin", mesh, WHITE);
}

} // namespace skin::vis
