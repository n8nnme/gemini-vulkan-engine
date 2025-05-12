#pragma once

#include <memory> // For std::shared_ptr

// It's generally good practice to include the specific headers from your logging library
// here if the macros directly depend on its types or functions not otherwise visible.
// For spdlog, including <spdlog/spdlog.h> itself is common.
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h> // For an ostream operator overload (optional, but good for custom types)

namespace VulkEng {

    class Log {
    public:
        // Initializes the logging system. Call once at application startup.
        static void Init();

        // Shuts down the logging system (optional, spdlog often handles this on exit).
        // static void Shutdown(); // Usually not needed with spdlog's default registry

        // Accessor for the core logger instance used by macros.
        // Returning a reference is common.
        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }

    private:
        // Static member to hold the core logger instance.
        // Needs to be defined in Log.cpp.
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
    };

} // namespace VulkEng

// --- Core Logging Macros ---
// These macros provide a convenient and consistent way to log messages.
// They use the core logger instance obtained from Log::GetCoreLogger().

// Trace level (most verbose, for detailed debugging)
#define VKENG_TRACE(...)    ::VulkEng::Log::GetCoreLogger()->trace(__VA_ARGS__)
// Info level (general information about application flow)
#define VKENG_INFO(...)     ::VulkEng::Log::GetCoreLogger()->info(__VA_ARGS__)
// Warning level (potential issues that don't stop execution)
#define VKENG_WARN(...)     ::VulkEng::Log::GetCoreLogger()->warn(__VA_ARGS__)
// Error level (recoverable errors that affect functionality)
#define VKENG_ERROR(...)    ::VulkEng::Log::GetCoreLogger()->error(__VA_ARGS__)
// Critical level (severe errors that likely lead to termination)
#define VKENG_CRITICAL(...) ::VulkEng::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Macro to log a warning message only once per call site.
// Useful for warnings in loops or frequently called functions.
#define VKENG_WARN_ONCE(...) \
    do { \
        static bool vkeng_logged_once_flag_at_this_site = false; \
        if (!vkeng_logged_once_flag_at_this_site) { \
            VKENG_WARN(__VA_ARGS__); \
            vkeng_logged_once_flag_at_this_site = true; \
        } \
    } while(0)


// --- Optional: Assertion Macros ---
// These can be enabled/disabled with a preprocessor definition.
#ifdef VKENG_ENABLE_ASSERTS
    // __debugbreak() is MSVC specific; use platform-specific alternatives or <cassert> assert().
    // For cross-platform, consider using <cassert> and `assert(expression);`
    #if defined(_MSC_VER)
        #define VKENG_DEBUGBREAK() __debugbreak()
    #elif defined(__clang__) || defined(__GNUC__)
        #define VKENG_DEBUGBREAK() __builtin_trap()
    #else
        #include <csignal>
        #define VKENG_DEBUGBREAK() std::raise(SIGTRAP) // Or simply use assert(false)
    #endif

    #define VKENG_ASSERT(condition, ...) \
        do { \
            if (!(condition)) { \
                VKENG_ERROR("Assertion Failed: {0}", __VA_ARGS__); \
                VKENG_DEBUGBREAK(); \
            } \
        } while(0)

    #define VKENG_CORE_ASSERT(condition, ...) \
        do { \
            if (!(condition)) { \
                VKENG_CRITICAL("Core Assertion Failed: {0}", __VA_ARGS__); \
                VKENG_DEBUGBREAK(); \
            } \
        } while(0)
#else
    #define VKENG_ASSERT(condition, ...)
    #define VKENG_CORE_ASSERT(condition, ...)
#endif // VKENG_ENABLE_ASSERTS











// 12 May 18:58 | what is wrong with it, why I should change every file because it's miss code that linked to another code that linked to code another code CODE BLYAT 
// 
// #pragma once
// 
// #include <memory>
// #include <spdlog/spdlog.h> // Include spdlog directly for macros
// 
// namespace VulkEng {
//     class Log {
//     public:
//         static void Init();
//         // Make logger accessible for macros
//         static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
// 
//     //private: // Make public or provide getter for macros
//         static std::shared_ptr<spdlog::logger> s_CoreLogger;
//     };
// }
// 
// // Core Logging Macros
// #define VKENG_TRACE(...)    ::VulkEng::Log::GetCoreLogger()->trace(__VA_ARGS__)
// #define VKENG_INFO(...)     ::VulkEng::Log::GetCoreLogger()->info(__VA_ARGS__)
// #define VKENG_WARN(...)     ::VulkEng::Log::GetCoreLogger()->warn(__VA_ARGS__)
// #define VKENG_ERROR(...)    ::VulkEng::Log::GetCoreLogger()->error(__VA_ARGS__)
// #define VKENG_CRITICAL(...) ::VulkEng::Log::GetCoreLogger()->critical(__VA_ARGS__)
// 
// #define VKENG_WARN_ONCE(...) \
//     do { \
//         static bool vkeng_logged_once_flag = false; \
//         if (!vkeng_logged_once_flag) { \
//             VKENG_WARN(__VA_ARGS__); \
//             vkeng_logged_once_flag = true; \
//         } \
//     } while(0)
