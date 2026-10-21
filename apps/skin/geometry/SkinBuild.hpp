#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <vector>

#include "skin/geometry/CgalSkin.hpp"
#include "skin/geometry/MixedComplex.hpp"
#include "skin/geometry/SkinMesh.hpp"
#include "skin/geometry/SkinQuadric.hpp"
#include "skin/geometry/WeightedPoint.hpp"

namespace skin::geometry {

enum class MeshingPhase : uint8_t {
    Starting,
    Refining,
    Triangulating,
    Marching,
    Cgal,
    Comparing,
    Statistics
};

// Shared between a meshing job and whoever watches it: the job reads the stop
// token and writes progress; the watcher only reads progress.
struct MeshingControl {
    std::stop_token stop;
    std::atomic<MeshingPhase> phase{MeshingPhase::Starting};
    std::atomic<float> fraction{-1.0f}; // of the current phase; < 0 when unknown
    std::atomic<size_t> tets{0};        // marching tets: refined so far

    bool stopRequested() const { return stop.stop_requested(); }
};

// A snapshot of MeshingControl for display.
struct MeshingStatus {
    bool busy = false;
    MeshingPhase phase = MeshingPhase::Starting;
    float fraction = -1.0f;
    size_t tets = 0;
    double seconds = 0.0;
};

// Copies of what meshing reads from the model, so a job on another thread is
// unaffected by the model being rebuilt meanwhile.
struct SkinInputs {
    std::vector<WeightedPoint> points;
    MixedComplex mixed;
    MixedComplexTopology topology;
    std::vector<SkinQuadric> quadrics;
};

struct SkinRequest {
    SkinMesher mesher = SkinMesher::KruithofVegter;
    double s = 0.5;
    double k = 1.0;       // marching tets refinement constant
    int subdivisions = 0; // CGAL subdivision rounds
};

struct SkinBuild {
    SkinMesh mesh;
    SkinMeshStats stats;
    SkinMesher mesher = SkinMesher::KruithofVegter;
    size_t tets = 0;                   // marching tets / KV: tets marched
    size_t monotonicityViolations = 0; // KV: Condition 17 failures (0 = isotopy holds)
    bool budgetExceeded = false;
    bool cancelled = false; // stopped early; the mesh is incomplete
    double milliseconds = 0.0;
};

// Meshes and logs a summary. Polls control.stop; CGAL itself can't be
// interrupted, only skipped before it starts.
SkinBuild buildSkin(const SkinInputs& inputs, const SkinRequest& request, MeshingControl& control);

// Meshes with ours (the selected mesher, Kruithof-Vegter when CGAL is
// selected) and CGAL, and compares; nullopt if stopped.
std::optional<SkinComparison> compareSkinMeshes(const SkinInputs& inputs,
                                                const SkinRequest& request,
                                                MeshingControl& control);

} // namespace skin::geometry
