# Computational Geometry in Skin Surface and Deformation

NUS School of Computing Final Year Project by George Yeo, supervised by
Prof. Ho-Lun (Alan) Cheng.

The project builds Edelsbrunner skin surfaces from weighted points, and later
deforms them. It is built on regular (weighted Delaunay) triangulations, their
dual power diagrams, and the mixed complex. The geometry runs live in
**Nacre**, a small C++23 / Vulkan engine written for this project. The FYP
application (`SkinSurfaceFYP`) sits on top of the engine.

## Status

Done:

- Regular triangulation and power diagram of a weighted point set (CGAL,
  exact predicates and constructions), with checks for Euler characteristic
  and orthogonality.
- Mixed cells μ_X for any shrink factor s ∈ (0, 1), clipped to a bounding box
  and checked by volume tiling.
- Live visualisation of all of the above, including inserting points
  interactively.

Next:

1. Per-cell skin data: centre z_X, weight w_X, local frame and bounding
   planes, with a check that neighbouring cells agree on shared walls.
2. A skin mesh by marching tetrahedra over the mixed complex
   (Kruithof–Vegter), compared against CGAL `Skin_surface_3`.
3. Cheng's restricted Delaunay meshing of the skin.
4. Deformation, using a dynamic version of (3).

## Background

A weighted point (ball) is b_i = (z_i, w_i), with w_i = r_i². For a simplex
X of the regular triangulation and its dual power-diagram face ν_X, the
mixed cell is

    μ_X = (1 − s)·X ⊕ s·ν_X,

and the mixed cells tile space. Inside μ_X, write a point as
x = z_X + u + v, where u is parallel to aff X and v is parallel to aff ν_X.
The skin in that cell is the quadric

    |v|² / s − |u|² / (1 − s) + w_X = 0.

It is a sphere patch in vertex and tetrahedron cells, and a hyperboloid
patch in edge and triangle cells. The pieces join with C¹ continuity across
cell walls.

## Building

Requirements:

- Windows with Visual Studio 2022 (MSVC, C++23) and its CMake integration
- [vcpkg](https://vcpkg.io) in manifest mode: `cgal`, `glm` and `glfw3` are
  listed in `vcpkg.json`
- [Vulkan SDK](https://vulkan.lunarg.com) 1.3 or newer, with `glslc` on `PATH`

To build in Visual Studio, open the folder. VS configures the `x64-Debug`
preset from `CMakeSettings.json` and writes to `out/build/x64-Debug/`. Then
build `SkinSurfaceFYP.exe`.

To build from a Developer shell, configure once:

```bash
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

Then build:

```bash
cmake --build out/build/x64-Debug
```

Notes:

- Warnings are errors (`/W4 /WX`; `-Wall -Wextra -Wpedantic -Werror` on
  other compilers).
- Sources are globbed with `CONFIGURE_DEPENDS`, so re-run CMake after adding
  or moving files.
- Shaders in `nacre/shaders/` are compiled to `<build>/shaders/*.spv` as part
  of the build.

## Running

Run the executable from the build directory. Shaders and the copied
`assets/` folder are found by paths relative to the working directory.

```bash
cd out/build/x64-Debug && ./SkinSurfaceFYP.exe
```

The input point set is `assets/test/points_simple.json`, loaded in
`SkinApp`. The format is a JSON array of weighted points, where
`weight` is the squared radius:

```json
[
  { "x": -3.0, "y": -2.0, "z": -2.0, "weight": 1.0 },
  { "x":  3.0, "y": -2.0, "z": -1.0, "weight": 1.0 }
]
```

Controls:

| Input | Action |
|---|---|
| W A S D / Q E | Move |
| Right-drag | Look |
| R | Reset camera |
| P | Insert a point under the cursor |

The ImGui debug panel toggles each layer and sets its opacity. The layers
are weights, Delaunay faces, edges and tetrahedra, Voronoi edges and cells,
mixed cells by dimension, a selected vertex's cell and the clip boxes. The
panel also sets the shrink factor s and the padding of the skin box and
display box.

## Repository layout

```
nacre/include/nacre/<module>/   engine public headers
nacre/src/<module>/             engine sources
nacre/shaders/                  GLSL
apps/skin/                      FYP application (SkinApp, camera)
apps/skin/geometry/             power complex, bounded Voronoi, mixed complex (no GPU deps)
apps/skin/vis/                  model → ECS layers, debug UI
assets/                         test point sets, meshes
docs/                           architecture diagram
external/                       vendored Dear ImGui, tinyobjloader, nlohmann/json
```

| Target | Kind | Contents |
|---|---|---|
| `nacre_core` | interface | core and shader-layout headers, glm configuration |
| `nacre` | static lib | engine: platform, ECS, Vulkan, assets, scene, renderer, app |
| `skin_geometry` | static lib | FYP geometry. It links CGAL privately; CGAL types never appear in its headers |
| `SkinSurfaceFYP` | executable | the FYP application |
| `Shaders` | custom | GLSL → SPIR-V via `glslc` |

The engine's modules form an acyclic stack:
core, shaders → platform, ecs, vk → assets → scene → render → app → skin.
See [`docs/architecture-layers.svg`](docs/architecture-layers.svg).

## Third-party code

| Library | Use | License |
|---|---|---|
| [CGAL](https://www.cgal.org) | regular triangulation | GPL-3.0-or-later / LGPL-3.0-or-later (per package) |
| [GLM](https://github.com/g-truc/glm) | maths | MIT |
| [GLFW](https://www.glfw.org) | windowing, input | zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | debug UI (vendored) | MIT |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ loading (vendored) | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | point-set parsing (vendored) | MIT |

CGAL's 3D Triangulations package, which `skin_geometry` uses, is GPL-3.0 (or
commercially licensed). The source in this repository is MIT, but a
distributed `SkinSurfaceFYP` binary is bound by CGAL's GPL terms.

## License

[MIT](LICENSE) © 2026 George Yeo
