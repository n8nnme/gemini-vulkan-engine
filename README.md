# VulkanEngine - A 3D Game Engine Skeleton

This project is a C++ 3D game engine skeleton built using the Vulkan 1.3 graphics API. It integrates several common libraries for windowing, UI, model loading, and physics.

## Features Implemented

*   **Core Engine Structure:**
    *   `Application` class orchestrating systems.
    *   `Engine` class as a high-level wrapper.
    *   Logging system (`spdlog`).
    *   Basic Service Locator pattern.
*   **Windowing & Input:**
    *   GLFW for window creation and input handling.
    *   `InputManager` for querying keyboard and mouse states.
*   **Graphics (Vulkan 1.3):**
    *   Explicit Vulkan setup: Instance, Device, Queues, Swapchain.
    *   Render pass and graphics pipeline for basic 3D rendering.
    *   Descriptor sets for UBOs (camera, lighting) and texture samplers.
    *   Push constants for per-object model matrices.
    *   Basic directional lighting (ambient + diffuse).
    *   Texture loading (`stb_image`) with mipmap generation and sampler caching.
    *   Depth testing.
*   **Asset Management:**
    *   `AssetManager` for loading and managing resources.
    *   Model loading for `.obj` and `.fbx` files using Assimp.
    *   `Mesh`, `Material`, `Texture` data structures.
*   **Scene System:**
    *   `Scene` class to manage `GameObject`s.
    *   `GameObject` class with a component-based architecture.
    *   Components: `TransformComponent`, `MeshComponent`, `CameraComponent`, `RigidBodyComponent`.
    *   Basic free-look camera controls (WASD, mouse).
*   **Physics:**
    *   Bullet Physics integration.
    *   `PhysicsSystem` managing the Bullet world.
    *   `RigidBodyComponent` for dynamic, static, and kinematic objects.
    *   Support for Box, Sphere, Capsule, Cylinder, and Triangle Mesh collision shapes.
    *   `EngineMotionState` for synchronizing engine transforms with Bullet.
    *   Collision event callbacks (`OnCollisionEnter`, `OnCollisionExit`).
*   **UI:**
    *   Dear ImGui integration for debug UI and potential editor tools.

## Libraries Used

*   **Vulkan SDK:** For the Vulkan graphics API.
*   **GLFW:** Windowing, input, Vulkan surface creation.
*   **GLM:** Mathematics library for graphics (vectors, matrices).
*   **Dear ImGui:** Immediate mode graphical user interface.
*   **Assimp:** Open Asset Import Library for loading 3D models.
*   **stb_image.h:** Single-header image loading library.
*   **Bullet Physics:** 3D collision detection and rigid body dynamics.
*   **spdlog:** Fast C++ logging library.

## Project Structure
```bash
VulkanEngine/
├── src/                  # Source code
│ ├── core/               # Core engine systems (Application, Window, Input, Log, ServiceLocator)
│ ├── graphics/           # Vulkan rendering subsystem (Renderer, Context, Swapchain, Buffers, etc.)
│ ├── scene/              # Scene graph and component system (Scene, GameObject, Components)
│ ├── assets/             # Asset loading and management (AssetManager, ModelLoader, asset structs)
│ ├── ui/                 # User Interface (UIManager for ImGui)
│ ├── physics/            # Physics system (PhysicsSystem, Bullet integration components)
│ └── main.cpp            # Main entry point
├── external/             # Placeholder for manually added libraries (e.g., stb_image.h)
│ └── stb/
├── assets/               # Game assets to be loaded
│ ├── models/             # .obj, .fbx files
│ │ └── textures/         # Texture files if not alongside models
│ └── shaders/            # GLSL shaders (.vert, .frag)
├── CMakeLists.txt        # CMake build configuration
└── README.md             # This file
```

## Building the Project

1.  **Prerequisites:**
    *   A C++17 (or newer) compliant compiler (e.g., GCC, Clang, MSVC).
    *   CMake (version 3.16 or newer recommended).
    *   Vulkan SDK installed and configured (ensure `VULKAN_SDK` environment variable is set and `glslc` is in PATH).
    *   Git (for `FetchContent` to download dependencies).

2.  **Clone the repository (if it were one):**
    ```bash
    # git clone https://github.com/n8nnme/gemini-vulkan-engine
    # cd gemini-vulkan-engine
    ```
    *(Since this is a code dump, you'll create the files manually, ANOTHER AI SOLUTION FOR SKELET JAB)*

3.  **Place `stb_image.h`:**
    *   Download `stb_image.h` from [https://github.com/nothings/stb](https://github.com/nothings/stb).
    *   Create a folder `external/stb/` in your project root.
    *   Place `stb_image.h` into `external/stb/`.

4.  **Configure and Build with CMake:**
    ```bash
    mkdir build
    cd build
    cmake .. 
    # On Windows, you might need to specify a generator, e.g., cmake .. -G "Visual Studio 17 2022"
    cmake --build . --config Release 
    # Or open the generated solution/project file in your IDE.
    ```
    The first CMake configuration step will download and build the dependencies (GLM, ImGui, Assimp, Bullet, Spdlog) using `FetchContent`. This may take some time.

5.  **Run:**
    *   The executable (`VulkanEngine` or `VulkanEngine.exe`) will be in the `build` directory (or a subdirectory like `build/Release`).
    *   Ensure the `assets` directory (containing models and compiled shaders) is accessible from where you run the executable. The `CMakeLists.txt` attempts to copy necessary assets to the build directory.

## How to Use

*   Place your 3D models (e.g., `.obj`, `.fbx`) in the `assets/models/` directory.
*   Place corresponding textures where the model files expect them (often in the same directory or a `textures` subdirectory).
*   Modify `Application::Initialize()` in `src/core/Application.cpp` to load your models and set up your scene.

## Known Issues / Limitations / Workarounds

*   **Null Service Objects & `skipInit`:** The Service Locator uses a Null Object pattern that relies on base classes (`Window`, `VulkanContext`, etc.) having constructors with `skipInit` flags. This is a workaround for not using a full interface-based design for services. This makes the constructors of these base classes slightly more complex.
*   **Asset Paths:** Asset loading currently assumes paths are relative to the executable's working directory, and that the `assets` folder structure is mirrored in the build output.
*   **Error Handling:** While basic `VK_CHECK` and logging are present, error handling could be more robust for production.
*   **Performance:** This is a skeleton; many performance optimizations typical of a production engine are not implemented (e.g., advanced culling, multi-threaded rendering/asset loading, sophisticated memory management via VMA).

## Future Development Ideas (thanks to gemini, that gemini do in future)

*   PBR Material System (Metallic-Roughness workflow).
*   Advanced Lighting (Spotlights, Point Lights, Shadows).
*   Skeletal Animation.
*   Scene Hierarchy (Parent-Child relationships for GameObjects).
*   Improved Frustum Culling.
*   Scripting Language Integration (e.g., Lua).
*   Vulkan Memory Allocator (VMA) integration.
*   Refactor Service Locator dependencies to use interfaces.

made by Gemini 2.5 Pro Preview 05-06. All copyright to their owners, I'm just owl

