#include "InputManager.h"
#include "Log.h" // For logging initialization/shutdown
#include <stdexcept> // For std::runtime_error

namespace VulkEng {

    // Initialize static members that require it (arrays are default-initialized with C++17 inline static)
    // glm::vec2 InputManager::s_CurrentMousePosition = {0.0f, 0.0f};
    // ...etc...


    void InputManager::Init(GLFWwindow* window) {
        if (!window) {
            VKENG_CRITICAL("InputManager::Init received a null GLFW window handle!");
            throw std::runtime_error("Cannot initialize InputManager without a window.");
        }
        s_TargetWindow = window;
        VKENG_INFO("InputManager: Initializing and registering GLFW callbacks...");

        // Store initial mouse position from GLFW
        double x, y;
        glfwGetCursorPos(s_TargetWindow, &x, &y);
        s_CurrentMousePosition = {(float)x, (float)y};
        s_LastMousePositionUpdate = s_CurrentMousePosition; // Initialize last pos for delta calc

        // Set GLFW callbacks to our static methods
        glfwSetKeyCallback(s_TargetWindow, GlfwKeyCallback);
        glfwSetMouseButtonCallback(s_TargetWindow, GlfwMouseButtonCallback);
        glfwSetCursorPosCallback(s_TargetWindow, GlfwCursorPosCallback);
        glfwSetScrollCallback(s_TargetWindow, GlfwScrollCallback);

        VKENG_INFO("InputManager: Initialized.");
    }

    void InputManager::Shutdown() {
        VKENG_INFO("InputManager: Shutting down...");
        // If we set callbacks, it's good practice to clear them if the window might persist
        // after InputManager is "shutdown" (though usually window is destroyed first).
        if (s_TargetWindow) {
            // glfwSetKeyCallback(s_TargetWindow, nullptr);
            // glfwSetMouseButtonCallback(s_TargetWindow, nullptr);
            // glfwSetCursorPosCallback(s_TargetWindow, nullptr);
            // glfwSetScrollCallback(s_TargetWindow, nullptr);
        }
        s_TargetWindow = nullptr;
        VKENG_INFO("InputManager: Shutdown complete.");
    }

    void InputManager::Update() {
        // 1. Copy current states to previous states
        s_PreviousKeyStates = s_CurrentKeyStates;
        s_PreviousMouseStates = s_CurrentMouseStates;

        // 2. Update mouse delta
        // s_LastMousePositionUpdate is the position at the *start* of this Update() call.
        // s_CurrentMousePosition has been updated by GlfwCursorPosCallback throughout the frame.
        s_MouseDeltaThisFrame = s_CurrentMousePosition - s_LastMousePositionUpdate;
        s_LastMousePositionUpdate = s_CurrentMousePosition; // Update for the next frame's delta calc

        // 3. Reset accumulated scroll delta (it's already been queried by GetScrollDelta)
        s_AccumulatedScrollDelta = {0.0f, 0.0f};

        // glfwPollEvents() should have been called *before* this Update() in the main loop,
        // so s_CurrentKeyStates etc. are up-to-date from GLFW callbacks.
    }

    // --- Keyboard State Queries ---
    bool InputManager::IsKeyPressed(int glfwKeyCode) {
        if (glfwKeyCode < 0 || glfwKeyCode >= MAX_KEYS) return false;
        return s_CurrentKeyStates[glfwKeyCode] && !s_PreviousKeyStates[glfwKeyCode];
    }

    bool InputManager::IsKeyDown(int glfwKeyCode) {
        if (glfwKeyCode < 0 || glfwKeyCode >= MAX_KEYS) return false;
        return s_CurrentKeyStates[glfwKeyCode];
    }

    bool InputManager::IsKeyReleased(int glfwKeyCode) {
        if (glfwKeyCode < 0 || glfwKeyCode >= MAX_KEYS) return false;
        return !s_CurrentKeyStates[glfwKeyCode] && s_PreviousKeyStates[glfwKeyCode];
    }

    // --- Mouse Button State Queries ---
    bool InputManager::IsMouseButtonPressed(int glfwMouseButtonCode) {
        if (glfwMouseButtonCode < 0 || glfwMouseButtonCode >= MAX_MOUSE_BUTTONS) return false;
        return s_CurrentMouseStates[glfwMouseButtonCode] && !s_PreviousMouseStates[glfwMouseButtonCode];
    }

    bool InputManager::IsMouseButtonDown(int glfwMouseButtonCode) {
        if (glfwMouseButtonCode < 0 || glfwMouseButtonCode >= MAX_MOUSE_BUTTONS) return false;
        return s_CurrentMouseStates[glfwMouseButtonCode];
    }

    bool InputManager::IsMouseButtonReleased(int glfwMouseButtonCode) {
        if (glfwMouseButtonCode < 0 || glfwMouseButtonCode >= MAX_MOUSE_BUTTONS) return false;
        return !s_CurrentMouseStates[glfwMouseButtonCode] && s_PreviousMouseStates[glfwMouseButtonCode];
    }

    // --- Mouse Position & Delta ---
    glm::vec2 InputManager::GetMousePosition() {
        return s_CurrentMousePosition; // Directly from GLFW callback
    }

    glm::vec2 InputManager::GetMouseDelta() {
        return s_MouseDeltaThisFrame; // Calculated in Update()
    }

    // --- Mouse Scroll ---
    glm::vec2 InputManager::GetScrollDelta() {
        return s_AccumulatedScrollDelta; // Accumulated by callback, returned, then reset in Update()
    }

    // --- GLFW Callback Implementations ---
    void InputManager::GlfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key >= 0 && key < MAX_KEYS) {
            if (action == GLFW_PRESS) {
                s_CurrentKeyStates[key] = true;
            } else if (action == GLFW_RELEASE) {
                s_CurrentKeyStates[key] = false;
            }
            // GLFW_REPEAT is ignored for this state model.
        }
        // Optional: Forward to registered event callbacks
        // for(const auto& callback : s_KeyActionCallbacks) { callback(key, scancode, action, mods); }
    }

    void InputManager::GlfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        if (button >= 0 && button < MAX_MOUSE_BUTTONS) {
            if (action == GLFW_PRESS) {
                s_CurrentMouseStates[button] = true;
            } else if (action == GLFW_RELEASE) {
                s_CurrentMouseStates[button] = false;
            }
        }
        // Optional: Forward to registered event callbacks
    }

    void InputManager::GlfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        // This callback updates the absolute position.
        // The delta is calculated in Update() based on the change from the previous Update()'s position.
        s_CurrentMousePosition.x = static_cast<float>(xpos);
        s_CurrentMousePosition.y = static_cast<float>(ypos);
        // Optional: Forward to registered event callbacks
    }

    void InputManager::GlfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        // Accumulate scroll events that might happen between frames/Update calls
        s_AccumulatedScrollDelta.x += static_cast<float>(xoffset);
        s_AccumulatedScrollDelta.y += static_cast<float>(yoffset);
        // Optional: Forward to registered event callbacks
    }

} // namespace VulkEng
