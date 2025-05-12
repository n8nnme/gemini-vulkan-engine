#pragma once

#include <string>
#include <functional>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace VulkEng {
    class Window {
    public:
        Window(int width, int height, const std::string& title, bool skipGlfwInit = false);
        virtual ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        virtual void PollEvents();
        virtual bool ShouldClose() const;
        virtual void GetFramebufferSize(int& width, int& height) const;
        virtual GLFWwindow* GetGLFWwindow() const { return m_Window; }
        virtual void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

        void SetCloseCallback(const std::function<void()>& callback);
        void SetResizeCallback(const std::function<void(int, int)>& callback);
        // Key/Mouse/Scroll callbacks are now set by InputManager directly

        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
        float GetAspectRatio() const { return (m_Height > 0) ? static_cast<float>(m_Width) / static_cast<float>(m_Height) : 1.0f; }
        bool WasFramebufferResized() { bool ret = m_FramebufferResized; m_FramebufferResized = false; return ret; }

    protected:
        GLFWwindow* m_Window = nullptr;
        int m_Width;
        int m_Height;
        std::string m_Title;
        bool m_FramebufferResized = false;

        std::function<void()> m_CloseCallback;
        std::function<void(int, int)> m_ResizeCallback;

    private:
        static void glfwErrorCallback(int error, const char* description);
        static void FramebufferSizeCallbackGLFW(GLFWwindow* window, int width, int height);
        static void WindowCloseCallbackGLFW(GLFWwindow* window);
    };
}
