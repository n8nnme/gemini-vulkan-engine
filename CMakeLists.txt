cmake_minimum_required(VERSION 3.16)
project(VulkanEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Prefer explicit standard features

# --- Find Packages ---
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# --- FetchContent for External Libraries ---
include(FetchContent)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking # Use docking branch for latest features like viewports
)
FetchContent_MakeAvailable(imgui)

# Assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.3.1
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT OFF CACHE BOOL "" FORCE) # Keep OFF, we link to its targets
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE) # GLTF is common
# Disable others you don't need to speed up Assimp's build
set(ASSIMP_BUILD_X_IMPORTER OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_BLEND_IMPORTER OFF CACHE BOOL "" FORCE)
# ...
FetchContent_MakeAvailable(assimp)

# Bullet Physics
set(BUILD_BULLET3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_PYBULLET OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "" FORCE) # For Windows, link dynamically to CRT
set(BULLET_USE_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE) # Use single precision floats
FetchContent_Declare(
    bullet3
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG 3.25
)
FetchContent_MakeAvailable(bullet3)

# Spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)


# --- Add Source Files ---
# Core
file(GLOB_RECURSE CORE_SOURCES src/core/*.cpp src/core/*.h)
# Graphics
file(GLOB_RECURSE GRAPHICS_SOURCES src/graphics/*.cpp src/graphics/*.h)
# Scene & Components
file(GLOB_RECURSE SCENE_SOURCES src/scene/*.cpp src/scene/*.h src/scene/Components/*.cpp src/scene/Components/*.h)
# Assets
file(GLOB_RECURSE ASSETS_SOURCES src/assets/*.cpp src/assets/*.h)
# UI
file(GLOB_RECURSE UI_SOURCES src/ui/*.cpp src/ui/*.h)
# Physics
file(GLOB_RECURSE PHYSICS_SOURCES src/physics/*.cpp src/physics/*.h)

set(ENGINE_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${GRAPHICS_SOURCES}
    ${SCENE_SOURCES}
    ${ASSETS_SOURCES}
    ${UI_SOURCES}
    ${PHYSICS_SOURCES}
)

# --- Create Executable ---
add_executable(VulkanEngine ${ENGINE_SOURCES})

# --- Target Include Directories ---
target_include_directories(VulkanEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${VULKAN_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${glm_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends # For imgui_impl_*.cpp
    ${assimp_SOURCE_DIR}/include # For Assimp headers
    ${assimp_BINARY_DIR}/include # For Assimp generated config.h
    ${bullet3_SOURCE_DIR}/src
    ${spdlog_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb # For stb_image.h
)

# --- Link Libraries ---
target_link_libraries(VulkanEngine PRIVATE
    Vulkan::Vulkan
    glfw
    glm                   # Target name from glm's CMake
    imgui                 # Target name from imgui's CMake
    Assimp::Assimp        # Target name from assimp's CMake
    BulletDynamics_ गोली # (BulletDynamics_MINIMAL for example, depends on bullet build) or specific Bullet libs
    BulletCollision_ गोली
    LinearMath_ गोली
    spdlog::spdlog        # Target name from spdlog's CMake
)
# Note: Bullet target names might be BulletDynamics, BulletCollision, LinearMath if built with default options.
# If Bullet is built as part of your project with custom options, the target names might vary.
# Using _ गोली suffix for now as FetchContent sometimes creates targets with this suffix.
# Check Bullet's generated CMake files in your build directory if linking fails.
# Common Bullet targets: BulletDynamics, BulletCollision, LinearMath, BulletSoftBody, Bullet3Common


# --- ImGui Integration ---
target_sources(VulkanEngine PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    # If imgui target doesn't include core files (it usually does):
    # ${imgui_SOURCE_DIR}/imgui.cpp
    # ${imgui_SOURCE_DIR}/imgui_draw.cpp
    # ${imgui_SOURCE_DIR}/imgui_tables.cpp
    # ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_compile_definitions(VulkanEngine PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK=0)


# --- Shader Compilation ---
find_program(GLSL_COMPILER glslc)
if(NOT GLSL_COMPILER)
    message(FATAL_ERROR "glslc compiler not found. Please ensure the Vulkan SDK is installed and in PATH.")
endif()

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/assets/shaders)

file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

function(CompileShader SHADER_NAME)
    set(INPUT_FILE ${SHADER_SOURCE_DIR}/${SHADER_NAME})
    set(OUTPUT_FILE ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${GLSL_COMPILER} ${INPUT_FILE} -o ${OUTPUT_FILE}
        DEPENDS ${INPUT_FILE}
        COMMENT "Compiling ${SHADER_NAME} to SPIR-V"
    )
    list(APPEND COMPILED_SHADER_FILES ${OUTPUT_FILE})
    set(COMPILED_SHADER_FILES ${COMPILED_SHADER_FILES} PARENT_SCOPE)
endfunction()

CompileShader(simple.vert)
CompileShader(simple.frag)

add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADER_FILES})
add_dependencies(VulkanEngine CompileShaders)

# Define path for runtime shader loading (relative to where executable runs from build dir)
target_compile_definitions(VulkanEngine PRIVATE SHADER_PATH_DEFINITION="\"assets/shaders/\"")


# --- Copy Assets to Build Directory (Optional, for convenience) ---
# This ensures that when you run from the build directory, assets are found.
# Adjust if your asset loading logic is different.
add_custom_command(TARGET VulkanEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/models
    ${CMAKE_BINARY_DIR}/assets/models
    COMMAND ${CMAKE_COMMAND} -E copy_directory # Copy textures if separate
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures
    ${CMAKE_BINARY_DIR}/assets/textures
    COMMENT "Copying model/texture assets to build directory"
)

# For MSVC, set the working directory for debugging runs in Visual Studio
if(MSVC)
    set_property(TARGET VulkanEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# Ensure Assimp's DLLs are copied to the executable directory on Windows
if(WIN32 AND ASSIMP_SHARED_LIBS) # ASSIMP_SHARED_LIBS might be defined by Assimp's CMake
    add_custom_command(TARGET VulkanEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Assimp::Assimp>" # Gets the path to the DLL
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<TARGET_FILE_NAME:Assimp::Assimp>"
        COMMENT "Copying Assimp DLL to output directory"
    )
endif()
