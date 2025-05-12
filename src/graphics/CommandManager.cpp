#include "CommandManager.h"
#include "VulkanContext.h" // Needs full definition for device, FindQueueFamilies
#include "VulkanUtils.h"   // For VK_CHECK
#include "core/Log.h"

#include <stdexcept> // For std::runtime_error, std::out_of_range

namespace VulkEng {

    CommandManager::CommandManager(VulkanContext& context, uint32_t frameCount, bool skipInit /*= false*/)
        : m_Context(context),
          m_CommandPool(VK_NULL_HANDLE),
          m_FrameCount(frameCount) // Store the frame count
    {
        if (skipInit) {
            VKENG_WARN("CommandManager: Skipping Vulkan Initialization for Dummy/Null instance!");
            // m_CommandBuffers will remain empty. m_CommandPool is VK_NULL_HANDLE.
            return;
        }

        // Ensure the context and device are valid before proceeding
        if (m_Context.device == VK_NULL_HANDLE) {
             VKENG_ERROR("CommandManager: Cannot initialize with a null Vulkan device in context!");
             throw std::runtime_error("CommandManager requires a valid Vulkan device.");
        }
        if (m_FrameCount == 0) {
            VKENG_ERROR("CommandManager: Frame count cannot be zero!");
            throw std::invalid_argument("CommandManager frame count must be greater than zero.");
        }


        VKENG_INFO("Creating Command Manager ({} command buffers for {} frames in flight)...", m_FrameCount, m_FrameCount);
        CreateCommandPool();
        if (m_CommandPool == VK_NULL_HANDLE) { // Should have been caught by VK_CHECK in CreateCommandPool
            throw std::runtime_error("Command pool creation failed in CommandManager constructor.");
        }
        CreateCommandBuffers(); // Creates m_FrameCount primary command buffers
        VKENG_INFO("Command Manager Created.");
    }

    CommandManager::~CommandManager() {
        // Command buffers are implicitly freed when the command pool is destroyed.
        if (m_CommandPool != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
            // VKENG_TRACE("Destroying Command Pool and its Command Buffers...");
            vkDestroyCommandPool(m_Context.device, m_CommandPool, nullptr);
            m_CommandPool = VK_NULL_HANDLE;
        }
        VKENG_INFO("Command Manager Destroyed.");
    }

    void CommandManager::CreateCommandPool() {
        QueueFamilyIndices queueFamilyIndices = m_Context.FindQueueFamilies(m_Context.physicalDevice);
        if (!queueFamilyIndices.graphicsFamily.has_value()) {
            throw std::runtime_error("Failed to find graphics queue family for command pool creation.");
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows individual command buffers to be reset.
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT is for pools whose buffers are short-lived.
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(m_Context.device, &poolInfo, nullptr, &m_CommandPool));
        VKENG_INFO("Vulkan Command Pool Created.");
    }

    void CommandManager::CreateCommandBuffers() {
        if (m_CommandPool == VK_NULL_HANDLE) {
            VKENG_ERROR("CommandManager::CreateCommandBuffers: Command pool is null!");
            throw std::runtime_error("Cannot create command buffers without a command pool.");
        }

        m_CommandBuffers.resize(m_FrameCount); // Allocate space for the command buffer handles

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary buffers can be submitted to a queue
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

        VK_CHECK(vkAllocateCommandBuffers(m_Context.device, &allocInfo, m_CommandBuffers.data()));
        VKENG_INFO("Allocated {} Primary Command Buffers.", m_CommandBuffers.size());
    }

    VkCommandBuffer CommandManager::BeginFrameRecording(uint32_t frameIndex) {
        if (m_CommandPool == VK_NULL_HANDLE) { // Check if properly initialized
            VKENG_ERROR("CommandManager::BeginFrameRecording: CommandManager not properly initialized (null pool).");
            return VK_NULL_HANDLE;
        }
        if (frameIndex >= m_CommandBuffers.size()) {
            VKENG_ERROR("CommandManager::BeginFrameRecording: Invalid frame index ({}) requested. Max is {}.", frameIndex, m_CommandBuffers.size() -1 );
            throw std::out_of_range("Invalid frame index for command buffer begin.");
        }

        VkCommandBuffer commandBuffer = m_CommandBuffers[frameIndex];

        // Reset the command buffer before starting to record new commands.
        // This allows the command buffer to be reused.
        VK_CHECK(vkResetCommandBuffer(commandBuffer, 0 /* Optional VkCommandBufferResetFlags */));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: Buffer will be submitted once and then reset.
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: Buffer can be resubmitted while pending execution (requires careful synchronization).
        // For per-frame buffers, ONE_TIME_SUBMIT is common as they are re-recorded each frame.
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr; // Only for secondary command buffers

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
        return commandBuffer;
    }

    void CommandManager::EndFrameRecording(uint32_t frameIndex) {
        if (m_CommandPool == VK_NULL_HANDLE) {
            VKENG_ERROR("CommandManager::EndFrameRecording: CommandManager not properly initialized (null pool).");
            return;
        }
        if (frameIndex >= m_CommandBuffers.size()) {
            VKENG_ERROR("CommandManager::EndFrameRecording: Invalid frame index ({}) requested. Max is {}.", frameIndex, m_CommandBuffers.size() -1 );
            throw std::out_of_range("Invalid frame index for command buffer end.");
        }

        VkCommandBuffer commandBuffer = m_CommandBuffers[frameIndex];
        VK_CHECK(vkEndCommandBuffer(commandBuffer));
    }

    VkCommandBuffer CommandManager::GetCommandBuffer(uint32_t frameIndex) const {
        if (frameIndex >= m_CommandBuffers.size()) {
            VKENG_ERROR("CommandManager::GetCommandBuffer: Invalid frame index ({}) requested. Max is {}.", frameIndex, m_CommandBuffers.size() -1 );
            throw std::out_of_range("Invalid command buffer frame index.");
        }
        return m_CommandBuffers[frameIndex];
    }

    // Note: Single-time command helpers (BeginSingleTimeCommands/EndSingleTimeCommands)
    // were moved to VulkanContext (as BeginSingleTimeCommandsHelper/EndSingleTimeCommandsHelper)
    // or could be in VulkanUtils, as they often need to be called with a specific command pool
    // which might be different from the CommandManager's main pool if using dedicated transfer queues.
    // If CommandManager's pool is used for them, then helper methods can remain here.
    // For simplicity, they were consolidated into VulkanContext to use its graphicsQueue and a passed pool.

} // namespace VulkEng





// 12 May 19:20 | ---
// #include "CommandManager.h"
// #include "VulkanContext.h"
// #include "VulkanUtils.h"
// #include "core/Log.h"
// 
// namespace VulkEng {
// 
//     CommandManager::CommandManager(VulkanContext& context, uint32_t frameCount, bool skipInit) // frameCount typically MAX_FRAMES_IN_FLIGHT
//         : m_Context(context), m_SwapChainImageCount(frameCount), m_CommandPool(VK_NULL_HANDLE)
//     {
//         if (skipInit) {
//             VKENG_WARN("CommandManager: Skipping Vulkan Initialization for Dummy/Null instance!");
//             return;
//         }
//         if (m_Context.device == VK_NULL_HANDLE) {
//              VKENG_ERROR("CommandManager: Cannot initialize with a null Vulkan device in context!");
//              throw std::runtime_error("Null device for CommandManager");
//         }
// 
//         VKENG_INFO("Creating Command Manager ({} command buffers)...", frameCount);
//         CreateCommandPool();
//         if (m_CommandPool == VK_NULL_HANDLE) throw std::runtime_error("Command pool creation failed.");
//         CreateCommandBuffers(); // Uses m_SwapChainImageCount for number of buffers
//         VKENG_INFO("Command Manager Created.");
//     }
// 
//     CommandManager::~CommandManager() {
//         if (m_CommandPool != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
//             // Command buffers are freed when the pool is destroyed
//             vkDestroyCommandPool(m_Context.device, m_CommandPool, nullptr);
//             m_CommandPool = VK_NULL_HANDLE;
//         }
//         VKENG_INFO("Command Manager Destroyed.");
//     }
// 
//     void CommandManager::CreateCommandPool() {
//         QueueFamilyIndices queueFamilyIndices = m_Context.FindQueueFamilies(m_Context.physicalDevice);
//         if (!queueFamilyIndices.graphicsFamily.has_value()){
//             throw std::runtime_error("Failed to find graphics queue family for command pool.");
//         }
// 
//         VkCommandPoolCreateInfo poolInfo{};
//         poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
//         poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
//         poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
// 
//         VK_CHECK(vkCreateCommandPool(m_Context.device, &poolInfo, nullptr, &m_CommandPool));
//     }
// 
//     void CommandManager::CreateCommandBuffers() {
//         m_CommandBuffers.resize(m_SwapChainImageCount); // Should be MAX_FRAMES_IN_FLIGHT typically
// 
//         VkCommandBufferAllocateInfo allocInfo{};
//         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//         allocInfo.commandPool = m_CommandPool;
//         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//         allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();
// 
//         VK_CHECK(vkAllocateCommandBuffers(m_Context.device, &allocInfo, m_CommandBuffers.data()));
//     }
// 
//     VkCommandBuffer CommandManager::BeginFrame(uint32_t frameIndex) { // frameIndex is 0 to MAX_FRAMES_IN_FLIGHT - 1
//         if (frameIndex >= m_CommandBuffers.size()) {
//             throw std::out_of_range("Invalid frame index for command buffer!");
//         }
//         VkCommandBuffer commandBuffer = m_CommandBuffers[frameIndex];
//         VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));
//         VkCommandBufferBeginInfo beginInfo{};
//         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Or 0 if reused more carefully
//         VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
//         return commandBuffer;
//     }
// 
//     void CommandManager::EndFrameRecording(uint32_t frameIndex) { // Renamed from EndFrame
//         if (frameIndex >= m_CommandBuffers.size()) {
//             throw std::out_of_range("Invalid frame index for command buffer end!");
//         }
//         VkCommandBuffer commandBuffer = m_CommandBuffers[frameIndex];
//         VK_CHECK(vkEndCommandBuffer(commandBuffer));
//     }
// 
//     // Single time command helpers are now in VulkanContext or VulkanUtils using a passed-in pool.
//     // Example: Utils::BeginSingleTimeCommands(m_Context.device, m_CommandPool);
// }
