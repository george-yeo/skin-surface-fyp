#include "skin/geometry/SkinBuild.hpp"

#include <chrono>
#include <string>
#include <utility>

#include "skin/geometry/KruithofVegter.hpp"
#include <nacre/core/Logger.hpp>

namespace skin::geometry {

namespace {

const char* mesherName(SkinMesher mesher) {
    switch (mesher) {
    case SkinMesher::Cgal:
        return "CGAL";
    case SkinMesher::KruithofVegter:
        return "Kruithof-Vegter";
    }
    return "";
}

} // namespace

SkinBuild buildSkin(const SkinInputs& inputs, const SkinRequest& request, MeshingControl& control) {
    const auto start = std::chrono::steady_clock::now();
    SkinBuild build;
    build.mesher = request.mesher;
    if (request.mesher == SkinMesher::KruithofVegter) {
        KvSkinResult result =
            meshSkinKruithofVegter(inputs.points, request.s, request.refinements, &control);
        build.mesh = std::move(result.mesh);
        build.tets = result.tets;
        build.monotonicityViolations = result.monotonicityViolations;
        build.isotopyTestFailures = result.isotopyTestFailures;
        build.cancelled = result.cancelled;
    } else if (control.stopRequested()) {
        build.cancelled = true;
    } else {
        control.phase = MeshingPhase::Cgal;
        control.fraction = -1.0f;
        build.mesh = meshSkinWithCgal(inputs.points, request.s, request.subdivisions);
    }
    if (build.cancelled) {
        return build;
    }

    control.phase = MeshingPhase::Statistics;
    control.fraction = -1.0f;
    build.stats = computeSkinMeshStats(build.mesh);
    build.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count();

    const SkinMeshStats& stats = build.stats;
    std::string chi;
    for (const int64_t value : stats.eulerCharacteristics) {
        chi += (chi.empty() ? "" : ", ") + std::to_string(value);
    }
    const nacre::core::Logger logger{"Skin"};
    logger.print("{}: {} V, {} E, {} F; {} components, chi [{}]; {} non-manifold edges; {} "
                 "tets; {:.1f} ms",
                 mesherName(request.mesher),
                 stats.vertices, stats.edges, stats.triangles, stats.eulerCharacteristics.size(),
                 chi, stats.nonManifoldEdges, build.tets, build.milliseconds);
    if (build.monotonicityViolations > 0) {
        logger.warn("Kruithof-Vegter: {} edges violate Condition 17", build.monotonicityViolations);
    }
    if (stats.nonManifoldEdges > 0) {
        logger.warn("Skin mesh is not watertight");
    }
    return build;
}

std::optional<SkinComparison> compareSkinMeshes(const SkinInputs& inputs,
                                                const SkinRequest& request,
                                                MeshingControl& control) {
    KvSkinResult ours =
        meshSkinKruithofVegter(inputs.points, request.s, request.refinements, &control);
    if (ours.cancelled || control.stopRequested()) {
        return std::nullopt;
    }
    control.phase = MeshingPhase::Cgal;
    control.fraction = -1.0f;
    const SkinMesh cgal = meshSkinWithCgal(inputs.points, request.s, request.subdivisions);
    if (control.stopRequested()) {
        return std::nullopt;
    }
    control.phase = MeshingPhase::Comparing;
    const SkinComparison comparison =
        compareSkins(ours.mesh, cgal, inputs.topology, inputs.quadrics);

    const nacre::core::Logger logger{"Skin"};
    logger.print("Compare (Kruithof-Vegter vs CGAL): ours {} V / {} F, CGAL {} V / {} F; {} / "
                 "{} CGAL vertices located; |F|/|grad F| max {:.3e}, mean {:.3e}; CGAL -> ours "
                 "max {:.3e}, mean {:.3e}; ours -> CGAL max {:.3e}, mean {:.3e}",
                 comparison.oursVertices, comparison.oursTriangles, comparison.cgalVertices,
                 comparison.cgalTriangles, comparison.located, comparison.cgalVertices,
                 comparison.maxResidual,
                 comparison.meanResidual, comparison.maxCgalToOurs, comparison.meanCgalToOurs,
                 comparison.maxOursToCgal, comparison.meanOursToCgal);
    return comparison;
}

} // namespace skin::geometry
