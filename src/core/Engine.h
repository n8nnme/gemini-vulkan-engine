#pragma once

#include "Application.h" // Include Application
#include <memory> // For std::unique_ptr

namespace VulkEng {

    class Engine {
    public:
        Engine();
        ~Engine();

        // Prevent copying
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Primary function to start and run the engine's application
        void Run();

        // Optional: Provide access to the application if needed externally,
        // though usually interaction is through the Engine's Run method.
        // Application* GetApplication() const { return m_Application.get(); }

    private:
        std::unique_ptr<Application> m_Application;
        bool m_IsInitialized = false;
    };

} // namespace VulkEng
