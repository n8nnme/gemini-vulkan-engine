#pragma once

#include "Window.h"
#include "graphics/Renderer.h"
#include "ui/UIManager.h"
#include "assets/AssetManager.h"
#include "scene/Scene.h"
#include "physics/PhysicsSystem.h"
// #include "graphics/CommandManager.h" // If App owns it, not Renderer

#include <memory> // For std::unique_ptr
#include <string>
#include <glm/glm.hpp> // For m_LastMousePos in camera controls

namespace VulkEng {

    class Application {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run();

    private:
        void Initialize();
        void MainLoop();
        void Cleanup();

        void HandleCameraInput(float deltaTime); // Helper for camera controls

        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Renderer> m_Renderer;
        // std::unique_ptr<CommandManager> m_CommandManager; // If Application owns it
        std::unique_ptr<AssetManager> m_AssetManager;
        std::unique_ptr<UIManager> m_UIManager;
        std::unique_ptr<PhysicsSystem> m_PhysicsSystem;
        std::unique_ptr<Scene> m_CurrentScene;

        bool m_IsRunning = true;
        float m_LastFrameTime = 0.0f;

        // Camera Control Members
        float m_CameraMoveSpeed = 5.0f;
        float m_CameraLookSpeed = 0.002f; // Adjusted sensitivity (radians per pixel)
        glm::vec2 m_LastMousePos = {0.0f, 0.0f};
        bool m_FirstMouse = true;
        bool m_MouseLookActive = false;
    };

} // namespace VulkEng
