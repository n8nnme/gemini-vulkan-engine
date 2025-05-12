#include "core/Engine.h"   // The main engine class that encapsulates the Application
#include "core/Log.h"      // For initializing the logging system
// No need to include ServiceLocator here as its use is managed within Application/Engine
// No need to include Application here directly as Engine manages it

#include <stdexcept> // For std::exception and std::runtime_error
#include <iostream>  // For potential fallback console output if logging fails catastrophically

int main(int argc, char* argv[]) {
    // 1. Initialize Core Systems (Logging is usually the very first)
    //    This ensures that any subsequent errors or info messages during engine
    //    startup can be properly logged.
    try {
        VulkEng::Log::Init();
    } catch (const spdlog::spdlog_ex& ex) {
        // If spdlog itself fails to initialize (e.g., file permission issues for file sink),
        // fallback to standard error output.
        std::cerr << "FATAL: Logging system initialization failed: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::cerr << "FATAL: Generic exception during logging system initialization: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "FATAL: Unknown exception during logging system initialization." << std::endl;
        return EXIT_FAILURE;
    }

    VKENG_INFO("------------------------------------------------------------");
    VKENG_INFO("               VulkEng - Vulkan Engine Startup              ");
    VKENG_INFO("------------------------------------------------------------");
    VKENG_INFO("main: Logging Initialized. Creating Engine instance...");


    // 2. Create and Run the Engine
    //    The Engine class constructor will create the Application instance,
    //    which in turn initializes all other subsystems (Window, Renderer, Assets, UI, Physics, Scene).
    //    The Engine::Run() method will then start the Application's main loop.
    //    The Application's destructor will handle cleanup of all its systems,
    //    including calling ServiceLocator::Reset().
    try {
        VulkEng::Engine engine; // Engine's constructor creates and initializes Application
        engine.Run();           // Starts the Application's main loop
        // Engine's destructor will be called here, which in turn calls Application's destructor
    }
    catch (const std::runtime_error& e) {
        // Catch runtime errors specifically, as they are often used for critical failures.
        VKENG_CRITICAL("FATAL RUNTIME ERROR in main: {}", e.what());
        // Fallback to cerr if logging is somehow compromised post-init
        std::cerr << "FATAL RUNTIME ERROR: " << e.what() << std::endl;
        // ServiceLocator::Reset() would have been called by Application's destructor if 'app' (inside 'engine') was constructed.
        // If Engine constructor itself failed, Application destructor might not run.
        // Consider if a global try-catch around engine creation/run in a separate scope is needed
        // to ensure ServiceLocator::Reset if Engine construction fails very early.
        // For now, Application's destructor is the primary point for Reset.
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        // Catch any other standard exceptions.
        VKENG_CRITICAL("FATAL STANDARD EXCEPTION in main: {}", e.what());
        std::cerr << "FATAL STANDARD EXCEPTION: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...) {
        // Catch-all for any other unknown exceptions.
        VKENG_CRITICAL("UNKNOWN FATAL EXCEPTION in main!");
        std::cerr << "UNKNOWN FATAL EXCEPTION!" << std::endl;
        return EXIT_FAILURE;
    }

    // 3. Successful Shutdown
    //    If we reach here, the engine ran and shut down (presumably) gracefully.
    //    Application's destructor should have handled all necessary cleanup, including
    //    ServiceLocator::Reset() and glfwTerminate().
    VKENG_INFO("------------------------------------------------------------");
    VKENG_INFO(" VulkEng - Engine Shutdown Successful. Exiting main.        ");
    VKENG_INFO("------------------------------------------------------------");

    // Optional: Explicitly shutdown spdlog if not handled by RAII or auto-registry drop.
    // spdlog::shutdown(); // Usually not necessary as spdlog cleans up on program exit.

    return EXIT_SUCCESS;
}


// 12 May 20:29 | another shot
// #include "core/Engine.h" // Use Engine class
// #include "core/Log.h"
// // #include "core/ServiceLocator.h" // ServiceLocator is managed within Application now
// 
// #include <stdexcept>
// 
// int main() {
//     VulkEng::Log::Init(); // Initialize Logging FIRST
//     VKENG_INFO("Starting VulkEng...");
// 
//     try {
//         VulkEng::Engine engine; // Create the engine instance
//         engine.Run();           // Run the engine (which runs the application)
//     } catch (const std::exception& e) {
//         // This catch might be redundant if Engine's constructor/Run handles it,
//         // but good for safety if Engine itself fails to construct.
//         VKENG_CRITICAL("Fatal Unhandled Exception in main: {}", e.what());
//         return EXIT_FAILURE;
//     } catch (...) {
//         VKENG_CRITICAL("Unknown Fatal Unhandled Exception in main!");
//         return EXIT_FAILURE;
//     }
// 
//     VKENG_INFO("VulkEng Shutdown Successful.");
//     return EXIT_SUCCESS;
// }









// 12 May 18:49 | It is because AI tells me to paste another code
// #include "core/Application.h"
// #include "core/Log.h"
// #include "core/ServiceLocator.h"
// #include <stdexcept>
// 
// int main() {
//     VulkEng::Log::Init(); // Initialize Logging FIRST
//     VKENG_INFO("Starting Vulkan Engine...");
// 
//     try {
//         VulkEng::Application app; // Constructor calls Initialize
//         app.Run();
//         // app.Cleanup() is called by Application destructor
//     } catch (const std::exception& e) {
//         VKENG_CRITICAL("Unhandled Exception in main: {}", e.what());
//         // ServiceLocator::Reset(); // Called by Application destructor if app was constructed
//         return EXIT_FAILURE;
//     } catch (...) {
//         VKENG_CRITICAL("Unknown Unhandled Exception in main!");
//         // ServiceLocator::Reset();
//         return EXIT_FAILURE;
//     }
// 
//     VKENG_INFO("Engine Shutdown Successful.");
//     return EXIT_SUCCESS;
// }
