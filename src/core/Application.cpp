#include "Application.h"
#include "Log.h"
#include "ServiceLocator.h"
#include "InputManager.h"

// Scene and Component Includes
#include "scene/GameObject.h"
#include "scene/Components/TransformComponent.h"
#include "scene/Components/MeshComponent.h"
#include "scene/Components/CameraComponent.h"
#include "scene/Components/RigidBodyComponent.h"
#include "assets/ModelLoader.h" // For LoadedModelData

#include <GLFW/glfw3.h>   // For time and key codes
#include <glm/gtc/constants.hpp> // For pi() in camera
#include <glm/common.hpp> // For glm::min/max/clamp

// For ImGui calls
#include <imgui.h>

namespace VulkEng {

    Application::Application() {
        // Log::Init() is called in main.cpp before Application constructor
        Initialize();
    }

    Application::~Application() {
        Cleanup();
    }

    void Application::Initialize() {
        VKENG_INFO("Initializing Application Systems...");

        // --- System Creation Order ---
        m_Window = std::make_unique<Window>(1280, 720, "Vulkan Engine");
        m_Renderer = std::make_unique<Renderer>(*m_Window);
        // Assuming Renderer creates/owns its primary CommandManager.
        // If App owned it: m_CommandManager = std::make_unique<CommandManager>(m_Renderer->GetContext(), MAX_FRAMES_IN_FLIGHT);
        // And Renderer constructor would take CommandManager&.

        m_AssetManager = std::make_unique<AssetManager>(m_Renderer->GetContext(), m_Renderer->GetCommandManagerInstance());
        m_UIManager = std::make_unique<UIManager>(*m_Window, m_Renderer->GetContext(), m_Renderer->GetMainRenderPass());
        m_PhysicsSystem = std::make_unique<PhysicsSystem>();
        m_CurrentScene = std::make_unique<Scene>();
        InputManager::Init(m_Window->GetGLFWwindow());

        // --- Service Locator: Provide all created services ---
        VKENG_INFO("Providing services to ServiceLocator...");
        ServiceLocator::Provide(m_Renderer.get());
        ServiceLocator::Provide(m_AssetManager.get());
        ServiceLocator::Provide(m_PhysicsSystem.get());
        ServiceLocator::Provide(m_UIManager.get());
        VKENG_INFO("Services Provided.");

        // --- Scene Setup ---
        VKENG_INFO("Setting up initial scene...");
        // Create Camera
        auto cameraObject = m_CurrentScene->CreateGameObject("MainCamera");
        auto* camTransform = cameraObject->AddComponent<TransformComponent>();
        camTransform->SetPosition(glm::vec3(0.0f, 2.0f, 5.0f)); // Adjusted camera start
        camTransform->LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        auto* camComponent = cameraObject->AddComponent<CameraComponent>();
        int width, height;
        m_Window->GetFramebufferSize(width, height);
        camComponent->SetPerspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
        m_CurrentScene->SetMainCamera(cameraObject);
        m_LastMousePos = InputManager::GetMousePosition(); // Initialize for camera controls

        // Create Floor
        auto floorObject = m_CurrentScene->CreateGameObject("Floor");
        auto* floorTransform = floorObject->AddComponent<TransformComponent>();
        floorTransform->SetPosition({0.0f, -1.0f, 0.0f});
        floorTransform->SetScale({20.0f, 1.0f, 20.0f}); // Make it 1 unit thick
        // TODO: Add visual for floor (e.g., a large textured quad/cube MeshComponent)
        RigidBodySettings floorSettings;
        floorSettings.mass = 0.0f; // Static
        floorSettings.shapeType = CollisionShapeType::BOX;
        floorSettings.dimensions = floorTransform->GetScale() * 0.5f; // Half-extents
        auto* floorRb = floorObject->AddComponent<RigidBodyComponent>(floorSettings);
        floorRb->InitializePhysics(m_PhysicsSystem.get());

        // Create Dynamic Box
        auto boxObject = m_CurrentScene->CreateGameObject("DynamicBox");
        auto* boxTransform = boxObject->AddComponent<TransformComponent>();
        boxTransform->SetPosition({0.5f, 5.0f, 0.5f});
        // TODO: Add visual for box (e.g., a cube MeshComponent)
        RigidBodySettings boxSettings;
        boxSettings.mass = 1.0f;
        boxSettings.shapeType = CollisionShapeType::BOX;
        boxSettings.dimensions = {0.5f, 0.5f, 0.5f}; // 1x1x1 cube
        auto* boxRb = boxObject->AddComponent<RigidBodyComponent>(boxSettings);
        boxRb->InitializePhysics(m_PhysicsSystem.get());

        // Load Viking Room Model & Setup Physics
        try {
            // Ensure path is correct relative to executable in build directory (CMake copies assets)
            ModelHandle modelHandle = m_AssetManager->LoadModel("assets/models/viking_room.obj");
            if (modelHandle != InvalidModelHandle) {
                auto modelObject = m_CurrentScene->CreateGameObject("Viking Room");
                auto* modelTransform = modelObject->AddComponent<TransformComponent>();
                modelTransform->SetPosition({0.0f, -0.5f, 0.0f}); // Adjust Y slightly if needed
                modelTransform->SetEulerAngles({0.0f, glm::radians(90.0f), 0.0f}); // Example rotation
                modelTransform->SetScale({1.0f, 1.0f, 1.0f});

                const LoadedModelData* loadedData = m_AssetManager->GetLoadedModelData(modelHandle);
                bool geometryFetched = false;
                if (loadedData && !loadedData->allVerticesPhysics.empty() && !loadedData->allIndicesPhysics.empty()) {
                    RigidBodySettings modelSettings;
                    modelSettings.mass = 0.0f; // Static environment
                    modelSettings.shapeType = CollisionShapeType::TRIANGLE_MESH;
                    modelSettings.physicsVertices = loadedData->allVerticesPhysics;
                    modelSettings.physicsIndices = loadedData->allIndicesPhysics;
                    geometryFetched = true;

                    auto* modelRb = modelObject->AddComponent<RigidBodyComponent>(modelSettings);
                    modelRb->InitializePhysics(m_PhysicsSystem.get());
                } else {
                     VKENG_ERROR("Failed to retrieve valid physics geometry for Viking Room!");
                }

                auto& meshComp = modelObject->AddComponent<MeshComponent>();
                const auto& meshes = m_AssetManager->GetModelMeshes(modelHandle);
                for (const auto& mesh : meshes) {
                    meshComp.AddMesh(&mesh);
                }
            }
        } catch (const std::exception& e) {
            VKENG_ERROR("Model loading exception in Application::Initialize: {}", e.what());
        }
        VKENG_INFO("Initial scene setup complete.");

        // --- Event Handling ---
        m_Window->SetCloseCallback([this]() { m_IsRunning = false; });
        m_Window->SetResizeCallback([this](int w, int h) {
            if (w == 0 || h == 0) return;
            m_Renderer->HandleResize(w, h);
            if (auto* cam = m_CurrentScene->GetMainCamera()) {
                cam->SetPerspective(cam->GetFov(), static_cast<float>(w) / static_cast<float>(h), cam->GetNearPlane(), cam->GetFarPlane());
            }
        });

        VKENG_INFO("Application Initialized.");
    }

    void Application::Run() {
        m_LastFrameTime = static_cast<float>(glfwGetTime()); // Initialize last frame time
        while (m_IsRunning) {
            MainLoop();
        }
    }

    void Application::HandleCameraInput(float deltaTime) {
        if (!m_CurrentScene) return;
        auto* camTransform = m_CurrentScene->GetMainCameraTransform();
        if (!camTransform) return;

        // Toggle Mouse Look
        if (InputManager::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            m_MouseLookActive = !m_MouseLookActive;
            glfwSetInputMode(m_Window->GetGLFWwindow(), GLFW_CURSOR,
                             m_MouseLookActive ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (m_MouseLookActive) {
                m_FirstMouse = true; // Reset on activation
            }
        }

        if (m_MouseLookActive) {
            glm::vec2 currentMousePos = InputManager::GetMousePosition();
            if (m_FirstMouse) {
                m_LastMousePos = currentMousePos;
                m_FirstMouse = false;
            }
            glm::vec2 mouseDelta = currentMousePos - m_LastMousePos;
            m_LastMousePos = currentMousePos;

            float deltaYaw = -mouseDelta.x * m_CameraLookSpeed; // Invert X for typical FPS controls
            float deltaPitch = -mouseDelta.y * m_CameraLookSpeed;

            // Get current Euler angles to modify yaw and pitch
            glm::vec3 currentEuler = camTransform->GetEulerAngles(); // Radians
            currentEuler.y += deltaYaw; // Yaw around Y
            currentEuler.x += deltaPitch; // Pitch around X

            // Clamp pitch
            const float maxPitch = glm::radians(89.0f);
            currentEuler.x = glm::clamp(currentEuler.x, -maxPitch, maxPitch);

            camTransform->SetEulerAngles(currentEuler);
        } else {
            m_FirstMouse = true;
        }

        // Keyboard Movement
        glm::vec3 moveDir(0.0f);
        if (InputManager::IsKeyDown(GLFW_KEY_W)) moveDir += camTransform->GetForward();
        if (InputManager::IsKeyDown(GLFW_KEY_S)) moveDir -= camTransform->GetForward();
        if (InputManager::IsKeyDown(GLFW_KEY_A)) moveDir -= camTransform->GetRight();
        if (InputManager::IsKeyDown(GLFW_KEY_D)) moveDir += camTransform->GetRight();
        if (InputManager::IsKeyDown(GLFW_KEY_SPACE)) moveDir += glm::vec3(0.0f, 1.0f, 0.0f); // World Up
        if (InputManager::IsKeyDown(GLFW_KEY_LEFT_CONTROL)) moveDir -= glm::vec3(0.0f, 1.0f, 0.0f); // World Down

        if (glm::length(moveDir) > glm::epsilon<float>()) {
            camTransform->Translate(glm::normalize(moveDir) * m_CameraMoveSpeed * deltaTime);
        }
    }


    void Application::MainLoop() {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_LastFrameTime;
        m_LastFrameTime = currentTime;
        deltaTime = glm::min(deltaTime, 0.1f); // Clamp delta time

        // --- Poll Events (Must be first for input) ---
        m_Window->PollEvents();
        if (!m_IsRunning || m_Window->ShouldClose()) { // Check after polling
            m_IsRunning = false;
            return;
        }

        // --- Handle Input (Camera controls, game actions) ---
        HandleCameraInput(deltaTime); // Process camera movement/look
        if (InputManager::IsKeyPressed(GLFW_KEY_ESCAPE)) {
             if (m_MouseLookActive) {
                  m_MouseLookActive = false;
                  glfwSetInputMode(m_Window->GetGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                  m_FirstMouse = true; // Reset first mouse when deactivating look
             } else {
                  m_IsRunning = false;
             }
        }
        // Add other game-specific input checks here

        // --- Physics Update ---
        if (m_PhysicsSystem) m_PhysicsSystem->Update(deltaTime);

        // --- Game Logic Update ---
        if (m_CurrentScene) m_CurrentScene->Update(deltaTime); // Updates camera view matrix, component logic

        // --- ImGui Frame ---
        if (m_UIManager) {
            m_UIManager->BeginUIRender();
            // --- Build ImGui UI ---
            ImGui::Begin("Debug Info");
            ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", deltaTime * 1000.0f, deltaTime > 0 ? (1.0f / deltaTime) : 0.0f);
            if(auto* camT = m_CurrentScene->GetMainCameraTransform()){
                ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", camT->GetPosition().x, camT->GetPosition().y, camT->GetPosition().z);
            }
            // Add other ImGui elements
            ImGui::End();
            // --- Finish UI ---
            m_UIManager->EndUIRender(); // Calls ImGui::Render()
        }


        // --- Rendering ---
        if (m_Renderer && m_Renderer->BeginFrame()) {
            // Collect Renderables
            std::vector<RenderObjectInfo> renderables;
            CameraComponent* camera = m_CurrentScene ? m_CurrentScene->GetMainCamera() : nullptr;
            if (m_CurrentScene) {
                for (const auto& go : m_CurrentScene->GetAllGameObjects()) {
                    auto* meshComp = go->GetComponent<MeshComponent>();
                    auto* transformComp = go->GetComponent<TransformComponent>();
                    if (meshComp && transformComp) {
                        // TODO: Add Culling
                        for (const auto* meshPtr : meshComp->GetMeshes()) {
                            renderables.push_back({const_cast<Mesh*>(meshPtr), transformComp});
                        }
                    }
                }
            }

            m_Renderer->RecordCommands(renderables, camera); // Renderer calls UIManager::RenderDrawData internally
            m_Renderer->EndFrameAndPresent();
        }

        // --- Update Input Manager State (End of frame) ---
        InputManager::Update();
    }


    void Application::Cleanup() {
        VKENG_INFO("Cleaning up Application...");
        if (m_Renderer) m_Renderer->WaitForDeviceIdle();

        if (m_CurrentScene && m_PhysicsSystem) {
            VKENG_INFO("Cleaning up RigidBody Components...");
            for (const auto& go : m_CurrentScene->GetAllGameObjects()) {
                if (auto* rbComp = go->GetComponent<RigidBodyComponent>()) {
                    rbComp->CleanupPhysics(m_PhysicsSystem.get());
                }
            }
        }

        m_UIManager.reset(); VKENG_INFO("UIManager destroyed.");
        m_CurrentScene.reset(); VKENG_INFO("Scene destroyed.");
        m_PhysicsSystem.reset(); VKENG_INFO("PhysicsSystem destroyed.");
        m_AssetManager.reset(); VKENG_INFO("AssetManager destroyed.");
        m_Renderer.reset(); VKENG_INFO("Renderer destroyed.");

        InputManager::Shutdown(); VKENG_INFO("InputManager shutdown.");
        m_Window.reset(); VKENG_INFO("Window destroyed.");

        // Only terminate GLFW if it was initialized (which it should be if Window wasn't skipped)
        // The static bool in Window.cpp can also be used here for a more robust check.
        glfwTerminate();
        VKENG_INFO("GLFW Terminated.");

        ServiceLocator::Reset();
        VKENG_INFO("Application Cleanup Complete.");
    }

} // namespace VulkEng
