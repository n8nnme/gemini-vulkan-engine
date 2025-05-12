#include "Engine.h"
#include "Log.h" // For logging
#include <stdexcept> // For std::runtime_error

namespace VulkEng {

    Engine::Engine() {
        VKENG_INFO("VulkEng Engine Core Initializing...");
        // The primary initialization logic is within Application's constructor
        try {
            m_Application = std::make_unique<Application>();
            m_IsInitialized = true;
            VKENG_INFO("Application instance created within Engine.");
        } catch (const std::exception& e) {
            VKENG_CRITICAL("Failed to initialize Application within Engine: {}", e.what());
            // m_IsInitialized remains false
            // Rethrow or handle? For now, let Run() handle it.
            // Consider if Log::Init() should be called here if not in main().
            // However, main.cpp is a better place for Log::Init().
        } catch (...) {
             VKENG_CRITICAL("Unknown error during Application initialization within Engine.");
        }
    }

    Engine::~Engine() {
        // m_Application unique_ptr will automatically call Application's destructor,
        // which handles all cleanup.
        VKENG_INFO("VulkEng Engine Core Shutting Down...");
        // No explicit cleanup needed here if Application handles everything.
    }

    void Engine::Run() {
        if (!m_IsInitialized || !m_Application) {
            VKENG_CRITICAL("Engine cannot run: Application was not successfully initialized.");
            return;
        }

        try {
            VKENG_INFO("Engine::Run() - Starting Application main loop.");
            m_Application->Run();
            VKENG_INFO("Engine::Run() - Application main loop finished.");
        } catch (const std::exception& e) {
            VKENG_CRITICAL("Exception during Engine::Run (Application loop): {}", e.what());
            // Application might have already logged this if it came from its MainLoop.
        } catch (...) {
            VKENG_CRITICAL("Unknown exception during Engine::Run (Application loop).");
        }
    }

} // namespace VulkEng
