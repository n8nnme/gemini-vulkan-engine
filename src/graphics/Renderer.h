#pragma once

#include "VulkanContext.h" // Needs full VulkanContext definition
#include "Swapchain.h"
#include "CommandManager.h"
#include "graphics/Buffer.h" // For VulkanBuffer (used for UBOs)

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

// Forward Declarations
namespace VulkEng {
    class Window;
    struct Mesh;        // For RenderObjectInfo
    class CameraComponent; // For camera data
    class TransformComponent; // For RenderObjectInfo
    // class AssetManager; // If Renderer needs to interact directly (usually not for drawing)
    // class UIManager;    // If Renderer needs to interact directly (usually Application orchestrates)
}

namespace VulkEng {

    // Maximum number of frames that can be processed concurrently by the GPU.
    // Typically 2 (double buffering) or 3 (triple buffering).
    const int MAX_FRAMES_IN_FLIGHT = 2;

    // UBO struct for camera matrices (View and Projection)
    // Matches layout(set = 0, binding = 0) in vertex shader
    struct CameraMatricesUBO {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    // UBO struct for lighting data
    // Matches layout(set = 0, binding = 1) in fragment shader
    struct LightDataUBO {
         alignas(16) glm::vec4 direction; // w component often unused, or for type/intensity flag
         alignas(16) glm::vec4 color;     // rgb for color, a for intensity
    };

    // Struct to pass necessary information for rendering an object
    struct RenderObjectInfo {
        Mesh* mesh = nullptr;                // Pointer to the mesh data (vertices, indices, material handle)
        TransformComponent* transform = nullptr; // Pointer to the object's transform for model matrix
    };


    class Renderer {
    public:
        // Renderer constructor might take CommandManager if App owns it
        // Renderer(Window& window, CommandManager& commandManager);
        Renderer(Window& window); // Assuming Renderer creates its own CommandManager
        virtual ~Renderer(); // Make virtual if NullRenderer or other derived classes exist

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // --- Core Rendering Loop Methods ---
        // Prepares for rendering a new frame (acquires swapchain image, waits for fence)
        // Returns true if frame can be rendered, false if swapchain needs recreation.
        virtual bool BeginFrame();

        // Records all draw commands for the current frame.
        // Takes a list of objects to render and the active camera.
        virtual void RecordCommands(const std::vector<RenderObjectInfo>& renderables, CameraComponent* camera);

        // Submits the recorded command buffer and presents the frame.
        virtual void EndFrameAndPresent();


        // --- Utility & State Management ---
        // Handles window resize events (triggers swapchain recreation).
        virtual void HandleResize(int width, int height);
        // Waits for the GPU to finish all pending operations.
        virtual void WaitForDeviceIdle();

        VulkanContext& GetContext() { return *m_VulkanContext; } // Allow access to Vulkan context
        // Provides the current command buffer (for systems like UIManager to record into)
        virtual VkCommandBuffer GetCurrentCommandBuffer();
        // Provides the main render pass (needed by UIManager for ImGui integration)
        virtual VkRenderPass GetMainRenderPass() const { return m_RenderPass; }
        // Provides the command manager instance (e.g., for AssetManager buffer creation)
        virtual CommandManager& GetCommandManagerInstance() { return *m_CommandManager; }


    // Make members protected if derived classes (like NullRenderer) need direct access
    // Or provide protected getters. For now, keeping private as NullRenderer uses skipInit logic.
    private:
        friend class NullRenderer; // Allow NullRenderer to access private members if necessary for its dummy construction

        void InitVulkan(); // Main initialization sequence

        // --- Resource Creation ---
        void CreateSyncObjects();         // Semaphores and Fences for frame synchronization
        void CreateDescriptorSetLayouts();// For Set 0 (Frame: Camera, Light) and Set 1 (Material: Texture)
        void CreateUniformBuffers();      // For CameraMatricesUBO
        void CreateLightUniformBuffers(); // For LightDataUBO
        void CreateDescriptorPool();      // Pool for allocating descriptor sets
        void CreateFrameDescriptorSets(); // Descriptor sets for Set 0 (per frame in flight)
                                          // Material descriptor sets (Set 1) are created by AssetManager

        // Swapchain-dependent resources (recreated on resize)
        void CreateSwapchainDependents();
        void CreateRenderPass();
        void CreateGraphicsPipeline();    // Uses both descriptor set layouts for its pipeline layout
        void CreateDepthResources();
        void CreateFramebuffers();

        // --- Resource Cleanup ---
        void CleanupSwapchainDependents();
        void RecreateSwapchain();

        // --- Per-Frame Updates ---
        void UpdateCameraUBO(uint32_t currentFrameIndex, const glm::mat4& view, const glm::mat4& proj);
        void UpdateLightUBO(uint32_t currentFrameIndex);

        // Shader loading helpers
        static std::vector<char> ReadFile(const std::string& filename);
        VkShaderModule CreateShaderModule(const std::vector<char>& code);


        // --- Core Vulkan Members ---
        Window& m_Window;
        std::unique_ptr<VulkanContext> m_VulkanContext;
        std::unique_ptr<Swapchain> m_Swapchain;
        std::unique_ptr<CommandManager> m_CommandManager; // Renderer owns its CommandManager

        // --- Descriptor Set Layouts ---
        VkDescriptorSetLayout m_FrameDescriptorSetLayout = VK_NULL_HANDLE;    // For Set 0 (Camera + Light UBOs)
        VkDescriptorSetLayout m_MaterialDescriptorSetLayout = VK_NULL_HANDLE; // For Set 1 (Texture Sampler)

        // --- Pipeline Resources ---
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE; // Uses both frame and material layouts
        VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;

        // --- Render Pass & Framebuffers ---
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_SwapChainFramebuffers;

        // --- Depth Buffer Resources ---
        VkImage m_DepthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
        VkImageView m_DepthImageView = VK_NULL_HANDLE;
        VkFormat m_DepthFormat;

        // --- Uniform Buffers (one set per frame in flight for Set 0) ---
        std::vector<std::unique_ptr<VulkanBuffer>> m_UniformBuffers;      // For CameraMatricesUBO
        std::vector<std::unique_ptr<VulkanBuffer>> m_LightUniformBuffers; // For LightDataUBO

        // --- Descriptor Pool & Sets for Frame Data (Set 0) ---
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE; // Shared pool for frame and material sets
        std::vector<VkDescriptorSet> m_FrameDescriptorSets; // One set per frame in flight (for Set 0)


        // --- Synchronization Primitives ---
        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;

        // --- Frame State ---
        uint32_t m_CurrentFrameIndex = 0; // Index for sync objects (0 to MAX_FRAMES_IN_FLIGHT-1)
        uint32_t m_CurrentImageIndex = 0; // Index of the currently acquired swapchain image
        bool m_FramebufferResized = false;

        // --- Lighting State (Simple example) ---
        glm::vec3 m_LightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, -0.3f));
        glm::vec3 m_LightColor = glm::vec3(1.0f, 0.95f, 0.8f);
        float m_LightIntensity = 1.0f;
    };

} // namespace VulkEng
