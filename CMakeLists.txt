cmake_minimum_required(VERSION 3.23)
project(PhysicsEngine LANGUAGES CXX)

# -----------------------------
# C++ standard
# -----------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# Warnings
# -----------------------------
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# -----------------------------
# clang-tidy
# -----------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_CLANG_TIDY
        clang-tidy
        -checks=bugprone-*,performance-*,readability-*,modernize-*
        -warnings-as-errors=*
        -header-filter=.*
        -checks=-modernize-use-trailing-return-type
    )
endif()

# -----------------------------
# clangd / tooling
# -----------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------
# Dependencies
# -----------------------------
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(CGAL CONFIG REQUIRED)
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/external)
set(IMGUI_DIR ${EXTERNAL_DIR}/imgui)

# -----------------------------
# nacre_core: header-only nacre::core and nacre::shaders, plus the glm
# configuration every target must agree on
# -----------------------------
add_library(nacre_core INTERFACE)

target_include_directories(nacre_core INTERFACE
    ${CMAKE_SOURCE_DIR}/nacre/include
)

target_link_libraries(nacre_core INTERFACE
    glm::glm
)

target_compile_definitions(nacre_core INTERFACE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
)

# -----------------------------
# imgui (vendored)
# -----------------------------
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp

    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui SYSTEM PUBLIC
    ${EXTERNAL_DIR}
    ${IMGUI_DIR}
)

target_link_libraries(imgui PUBLIC
    Vulkan::Vulkan
    glfw
)

# -----------------------------
# Shaders: compiled to <build>/${NACRE_SHADER_DIR}, which ShaderCache reads
# relative to the working directory
# -----------------------------
set(NACRE_SHADER_DIR shaders)
set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/nacre/shaders)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/${NACRE_SHADER_DIR})

file(GLOB SHADERS CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
)

file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

set(SPIRV_SHADERS)

foreach(SHADER ${SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV ${SHADER_BINARY_DIR}/${FILE_NAME}.spv)

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC_EXECUTABLE} -std=450 ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME}"
    )

    list(APPEND SPIRV_SHADERS ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_SHADERS})

# -----------------------------
# nacre: the engine. Its only include root is nacre/include, so nothing in
# it can include app code.
# -----------------------------
file(GLOB_RECURSE NACRE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/nacre/src/*.cpp"
)

add_library(nacre STATIC ${NACRE_SOURCES})

target_link_libraries(nacre
    PUBLIC
        nacre_core
        Vulkan::Vulkan
        glfw
    PRIVATE
        imgui
)

target_include_directories(nacre SYSTEM PRIVATE
    ${EXTERNAL_DIR}
)

target_compile_definitions(nacre PRIVATE
    NACRE_SHADER_DIR="${NACRE_SHADER_DIR}"
)

add_dependencies(nacre Shaders)

# -----------------------------
# skin_geometry: CGAL-backed geometry, no GPU or window dependencies
# -----------------------------
file(GLOB SKIN_GEOMETRY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/apps/skin/geometry/*.cpp"
)

add_library(skin_geometry STATIC ${SKIN_GEOMETRY_SOURCES})

target_include_directories(skin_geometry PUBLIC
    ${CMAKE_SOURCE_DIR}/apps
)

target_include_directories(skin_geometry SYSTEM PRIVATE
    ${EXTERNAL_DIR}
)

target_link_libraries(skin_geometry
    PUBLIC
        nacre_core
    PRIVATE
        CGAL::CGAL
)

# -----------------------------
# SkinSurfaceFYP: the skin app
# -----------------------------
file(GLOB SKIN_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/apps/skin/*.cpp"
    "${CMAKE_SOURCE_DIR}/apps/skin/vis/*.cpp"
)

add_executable(SkinSurfaceFYP ${SKIN_APP_SOURCES})

target_link_libraries(SkinSurfaceFYP PRIVATE
    nacre
    skin_geometry
    imgui
)

add_custom_command(TARGET SkinSurfaceFYP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:SkinSurfaceFYP>/assets
)

# -----------------------------
# Visual Studio niceties
# -----------------------------
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT SkinSurfaceFYP)
