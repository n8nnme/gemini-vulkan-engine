#include "Window.h"
#include "Log.h"
#include <stdexcept>
#define GLFW_INCLUDE_VULKAN // Ensures Vulkan headers are included by GLFW
#include <GLFW/glfw3.h>

namespace VulkEng {

    static void glfwErrorCallback(int error, const char* description) {
        VKENG_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    Window::Window(int width, int height, const std::string& title, bool skipGlfwInit /*= false*/)
        : m_Width(width), m_Height(height), m_Title(title), m_Window(nullptr), m_FramebufferResized(false)
    {
        if (skipGlfwInit) {
            VKENG_WARN("Window: Skipping GLFW Initialization for Dummy/Null instance!");
            return;
        }

        VKENG_INFO("Creating Window '{0}' ({1}x{2})", title, width, height);

        glfwSetErrorCallback(glfwErrorCallback);

        static bool glfwInitialized = false;
        if (!glfwInitialized) {
            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW!");
            }
            glfwInitialized = true;
            VKENG_INFO("GLFW Initialized.");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_Window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!m_Window) {
            throw std::runtime_error("Failed to create GLFW window!");
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallbackGLFW);
        glfwSetWindowCloseCallback(m_Window, WindowCloseCallbackGLFW);
    }

    Window::~Window() {
        if (m_Window != nullptr) {
            VKENG_INFO("Destroying Window: {}", m_Title);
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        // glfwTerminate is called once in Application::Cleanup
    }

    void Window::PollEvents() {
        if (m_Window) glfwPollEvents();
    }

    bool Window::ShouldClose() const {
        return m_Window ? glfwWindowShouldClose(m_Window) : true;
    }

    void Window::GetFramebufferSize(int& width, int& height) const {
        if (m_Window) {
            glfwGetFramebufferSize(m_Window, &width, &height); // Query actual size from GLFW
            // Internal m_Width/m_Height are updated by callback
        } else {
            width = 0; height = 0;
        }
    }

    void Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
        if (!m_Window) {
            VKENG_ERROR("Cannot create surface without a valid GLFW window!");
            *surface = VK_NULL_HANDLE;
             throw std::runtime_error("Attempt to create surface on null window.");
        }
        VKENG_INFO("Creating Vulkan Surface");
        if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    void Window::SetCloseCallback(const std::function<void()>& callback) { m_CloseCallback = callback; }
    void Window::SetResizeCallback(const std::function<void(int, int)>& callback) { m_ResizeCallback = callback; }

    void Window::FramebufferSizeCallbackGLFW(GLFWwindow* glfwWin, int width, int height) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWin));
        if (!window) return;
        window->m_Width = width;
        window->m_Height = height;
        window->m_FramebufferResized = true;
        if (window->m_ResizeCallback) {
            window->m_ResizeCallback(width, height);
        }
    }

    void Window::WindowCloseCallbackGLFW(GLFWwindow* glfwWin) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(glfwWin));
        if (window && window->m_CloseCallback) {
            window->m_CloseCallback();
        }
    }
}
