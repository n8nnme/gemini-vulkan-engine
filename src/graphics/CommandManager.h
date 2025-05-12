#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept> // For std::runtime_error (optional, can use assertions)
#include <cstdint>   // For uint32_t

namespace VulkEng {

    // Forward declaration
    class VulkanContext;

    // Manages command pools and command buffers, typically one set per frame in flight.
    class CommandManager {
    public:
        // Constructor:
        // - context: Reference to the initialized VulkanContext.
        // - frameCount: Number of command buffers to create (usually MAX_FRAMES_IN_FLIGHT).
        // - skipInit: Flag for dummy/null object construction.
        CommandManager(VulkanContext& context, uint32_t frameCount, bool skipInit = false);
        // Virtual destructor for potential inheritance (e.g., DummyCommandManagerSL).
        virtual ~CommandManager();

        // Prevent copying and assignment.
        CommandManager(const CommandManager&) = delete;
        CommandManager& operator=(const CommandManager&) = delete;

        // --- Command Buffer Management ---
        // Resets and begins recording on the command buffer for the given frame index.
        // `frameIndex` should be between 0 and `frameCount - 1`.
        // Returns the command buffer ready for recording.
        virtual VkCommandBuffer BeginFrameRecording(uint32_t frameIndex); // Renamed for clarity

        // Ends recording on the command buffer for the given frame index.
        // This must be called before submitting the command buffer.
        virtual void EndFrameRecording(uint32_t frameIndex);

        // --- Accessors ---
        // Gets the underlying command pool. Useful for allocating temporary/single-use command buffers.
        virtual VkCommandPool GetCommandPool() const { return m_CommandPool; }

        // Gets the vector of primary command buffers (one per frame in flight).
        // Useful for direct submission or access by other systems.
        const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_CommandBuffers; }
        // Gets a specific command buffer by frame index.
        VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const;


    // Protected for access by derived classes (like DummyCommandManagerSL if needed)
    // or make private and remove friend declaration if not strictly necessary.
    protected:
        friend class DummyCommandManagerSL; // For NullServices, if it needs to access these

        // --- Internal Initialization ---
        void CreateCommandPool();
        void CreateCommandBuffers(); // Creates primary command buffers

        VulkanContext& m_Context; // Reference to the Vulkan context
        VkCommandPool m_CommandPool = VK_NULL_HANDLE; // Command pool for allocating buffers

        // Primary command buffers, one for each frame that can be "in flight".
        // Size is determined by `frameCount` (typically MAX_FRAMES_IN_FLIGHT).
        std::vector<VkCommandBuffer> m_CommandBuffers;
        uint32_t m_FrameCount; // Number of command buffers created (matches constructor arg)
    };

} // namespace VulkEng





// 12 May 19:19 | )
// #pragma once
// 
// #include <vulkan/vulkan.h>
// #include <vector>
// #include <stdexcept> // For runtime_error
// 
// namespace VulkEng {
// 
// class VulkanContext;
// 
// class CommandManager {
// public:
//     CommandManager(VulkanContext& context, uint32_t swapChainImageCount, bool skipInit = false);
//     virtual ~CommandManager();
// 
//     CommandManager(const CommandManager&) = delete;
//     CommandManager& operator=(const CommandManager&) = delete;
// 
//     virtual VkCommandBuffer BeginFrame(uint32_t frameIndex);
//     virtual void EndFrameRecording(uint32_t frameIndex); // Renamed for clarity
//     virtual VkCommandPool GetCommandPool() const { return m_CommandPool; }
// 
//     // These now use context directly for helpers
//     // virtual VkCommandBuffer BeginSingleTimeCommands();
//     // virtual void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
// 
//     const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_CommandBuffers; }
// 
// 
// protected:
//     friend class DummyCommandManagerSL; // For NullServices
//     void CreateCommandPool();
//     void CreateCommandBuffers();
// 
//     VulkanContext& m_Context;
//     VkCommandPool m_CommandPool = VK_NULL_HANDLE;
//     std::vector<VkCommandBuffer> m_CommandBuffers;
//     uint32_t m_SwapChainImageCount; // Or number of frames in flight
// };
// 
// }
