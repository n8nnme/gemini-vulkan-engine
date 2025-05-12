#include "Log.h"

// Include specific spdlog sink headers
#include <spdlog/sinks/stdout_color_sinks.h> // For console output with colors
#include <spdlog/sinks/basic_file_sink.h>    // For logging to a file (optional)

namespace VulkEng {

    // Definition of the static core logger instance.
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

    void Log::Init() {
        // Create a list of sinks (log destinations).
        std::vector<spdlog::sink_ptr> logSinks;

        // 1. Console Sink (stdout, multi-threaded safe, colored output)
        // For Windows, spdlog might use wincolor_sink automatically if available.
        // stdout_color_sink_mt is generally cross-platform.
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        // Set a pattern for console messages. Example: [Timestamp] LoggerName: Message
        // %^ and %$ are for color range.
        console_sink->set_pattern("%^[%T.%e] [%n] [%l]: %v%$"); // Added milliseconds %e
        logSinks.push_back(console_sink);

        // 2. Optional: File Sink (basic_file_sink_mt for multi-threaded safe)
        // Set to true to truncate the file each time the application starts.
        // try {
        //     auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("VulkEng.log", true);
        //     file_sink->set_pattern("[%T.%e] [%n] [%l]: %v"); // File pattern (no color codes)
        //     logSinks.push_back(file_sink);
        // } catch (const spdlog::spdlog_ex& ex) {
        //     // Fallback or log error to console if file sink fails
        //     // This early in init, our main logger isn't set up yet.
        //     fprintf(stderr, "Log file sink creation failed: %s\n", ex.what());
        // }


        // Create the core logger with the configured sinks.
        // "VulkEng" is the name of the logger.
        s_CoreLogger = std::make_shared<spdlog::logger>("VulkEng", begin(logSinks), end(logSinks));

        // Register the logger with spdlog's global registry (optional but good practice).
        // This allows retrieving it by name elsewhere if needed, though our static accessor is primary.
        spdlog::register_logger(s_CoreLogger);

        // Set the logging level for the core logger.
        // Available levels (from most to least verbose):
        // trace, debug, info, warn, error, critical, off
        s_CoreLogger->set_level(spdlog::level::trace); // Log everything during development.
                                                      // Change to spdlog::level::info or warn for release.

        // Set flushing behavior (optional).
        // Flush logs immediately on error or critical messages.
        s_CoreLogger->flush_on(spdlog::level::err);

        // Use the logger itself to announce initialization (now that it's created).
        if (s_CoreLogger) { // Check if logger was successfully created
            s_CoreLogger->info("Logging System Initialized (Level: {}).", spdlog::level::to_string_view(s_CoreLogger->level()));
        } else {
            // This should not happen if spdlog works correctly.
            fprintf(stderr, "FATAL: Core logger creation failed!\n");
        }
    }

    // Optional Shutdown method (spdlog usually handles cleanup automatically)
    // void Log::Shutdown() {
    //     VKENG_INFO("Logging System Shutting Down...");
    //     spdlog::shutdown(); // Drops all registered loggers and cleans up
    // }

} // namespace VulkEng













// 12 May 19:00 | okay, here we go again
// #include "Log.h"
// #include <spdlog/sinks/stdout_color_sinks.h>
// #include <spdlog/sinks/basic_file_sink.h>
// 
// namespace VulkEng {
//     std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
// 
//     void Log::Init() {
//         std::vector<spdlog::sink_ptr> logSinks;
//         logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
//         // logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("VulkEng.log", true));
// 
//         logSinks[0]->set_pattern("%^[%T] %n: %v%$");
//         // if (logSinks.size() > 1) logSinks[1]->set_pattern("[%T] [%l] %n: %v");
// 
//         s_CoreLogger = std::make_shared<spdlog::logger>("VulkEng", begin(logSinks), end(logSinks));
//         spdlog::register_logger(s_CoreLogger);
//         s_CoreLogger->set_level(spdlog::level::trace);
//         s_CoreLogger->flush_on(spdlog::level::err);
// 
//         // VKENG_INFO("Logging Initialized (Level: Trace)"); // Can't use macro before s_CoreLogger is set
//         Log::GetCoreLogger()->info("Logging Initialized (Level: Trace)");
//     }
// }
