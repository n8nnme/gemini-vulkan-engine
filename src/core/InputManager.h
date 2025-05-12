#pragma once

#include <GLFW/glfw3.h> // For GLFW_KEY_*, GLFW_MOUSE_BUTTON_*, etc.
#include <glm/glm.hpp>  // For glm::vec2
#include <array>        // For std::array to store key/button states
#include <vector>       // For storing event callbacks (optional)
#include <functional>   // For std::function (optional, for event callbacks)

namespace VulkEng {

    class InputManager {
    public:
        // Deleted constructor/destructor for a static class
        InputManager() = delete;
        ~InputManager() = delete;

        // --- Initialization & Shutdown ---
        // Called once: sets up GLFW callbacks.
        static void Init(GLFWwindow* window);
        // Called once: cleans up (though often not much to do for a static input manager if window handles callback removal).
        static void Shutdown();

        // --- Per-Frame Update ---
        // Called ONCE per frame, typically at the very END of the main loop
        // (after processing input for the current frame and before the next PollEvents).
        // This updates the "pressed" and "released" states.
        static void Update();

        // --- Keyboard State Queries ---
        // Returns true if the key was just pressed down THIS frame.
        static bool IsKeyPressed(int glfwKeyCode);
        // Returns true if the key is currently held down.
        static bool IsKeyDown(int glfwKeyCode);
        // Returns true if the key was just released THIS frame.
        static bool IsKeyReleased(int glfwKeyCode);

        // --- Mouse Button State Queries ---
        static bool IsMouseButtonPressed(int glfwMouseButtonCode);
        static bool IsMouseButtonDown(int glfwMouseButtonCode);
        static bool IsMouseButtonReleased(int glfwMouseButtonCode);

        // --- Mouse Position & Delta ---
        // Returns the current mouse cursor position in screen coordinates.
        static glm::vec2 GetMousePosition();
        // Returns the change in mouse position since the last frame.
        static glm::vec2 GetMouseDelta();

        // --- Mouse Scroll ---
        // Returns the scroll offset (x, y) accumulated THIS frame.
        static glm::vec2 GetScrollDelta();

        // --- Optional: Event-Based Callbacks (more advanced) ---
        // using KeyActionCallback = std::function<void(int key, int scancode, int action, int mods)>;
        // static void RegisterKeyActionCallback(KeyActionCallback callback);
        // ... similar for mouse buttons, cursor, scroll ...

    private:
        // --- GLFW Callback Implementations (Static) ---
        static void GlfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void GlfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void GlfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void GlfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

        // --- Internal State ---
        static constexpr int MAX_KEYS = GLFW_KEY_LAST + 1;
        static constexpr int MAX_MOUSE_BUTTONS = GLFW_MOUSE_BUTTON_LAST + 1;

        // Current state of keys/buttons (true if down, false if up)
        inline static std::array<bool, MAX_KEYS> s_CurrentKeyStates{};          // Initialize to false
        inline static std::array<bool, MAX_MOUSE_BUTTONS> s_CurrentMouseStates{}; // Initialize to false

        // State of keys/buttons in the PREVIOUS frame
        inline static std::array<bool, MAX_KEYS> s_PreviousKeyStates{};
        inline static std::array<bool, MAX_MOUSE_BUTTONS> s_PreviousMouseStates{};

        inline static glm::vec2 s_CurrentMousePosition = {0.0f, 0.0f};
        inline static glm::vec2 s_LastMousePositionUpdate = {0.0f, 0.0f}; // For delta calculation within Update()
        inline static glm::vec2 s_MouseDeltaThisFrame = {0.0f, 0.0f}; // Mouse delta calculated in Update()

        // Scroll delta is accumulated from multiple GLFW scroll events within one frame
        inline static glm::vec2 s_AccumulatedScrollDelta = {0.0f, 0.0f};

        inline static GLFWwindow* s_TargetWindow = nullptr; // Store the window we're listening to

        // Optional: Storage for event-based callbacks
        // inline static std::vector<KeyActionCallback> s_KeyActionCallbacks;
    };

} // namespace VulkEng
