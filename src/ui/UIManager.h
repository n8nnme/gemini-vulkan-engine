#pragma once

#include <vulkan/vulkan.h> // For VkCommandBuffer, VkRenderPass, VkDescriptorPool

// Forward declare ImGui types if not including imgui.h here (though it's often included)
// struct ImDrawData; // Included by imgui_impl_vulkan.h which UIManager.cpp will include

namespace VulkEng {

    // Forward declarations from our engine
    class Window;
    class VulkanContext;

    // Manages the ImGui user interface integration with Vulkan and GLFW.
    class UIManager {
    public:
        // Constructor:
        // - window: Reference to the application's main window (for GLFW backend).
        // - context: Reference to the Vulkan context (for Vulkan backend).
        // - renderPass: The main render pass into which ImGui will render. ImGui's pipeline
        //               must be compatible with this render pass.
        // - skipInit: Flag for dummy/null object construction.
        UIManager(Window& window, VulkanContext& context, VkRenderPass renderPass, bool skipInit = false);
        // Virtual destructor for potential inheritance (e.g., NullUIManager).
        virtual ~UIManager();

        // Prevent copying and assignment.
        UIManager(const UIManager&) = delete;
        UIManager& operator=(const UIManager&) = delete;


        // --- ImGui Frame Management (called from Application's main loop) ---

        // Begins a new ImGui frame. Call this after polling events and before building UI.
        // This calls ImGui_ImplVulkan_NewFrame, ImGui_ImplGlfw_NewFrame, and ImGui::NewFrame.
        virtual void BeginUIRender();

        // Ends the current ImGui frame. Call this after all ImGui UI elements have been defined.
        // This primarily calls ImGui::Render() to finalize ImGui's internal draw data.
        // The actual Vulkan draw commands are recorded by RenderDrawData.
        virtual void EndUIRender();


        // --- Vulkan Draw Command Recording ---

        // Records ImGui's draw commands into the provided Vulkan command buffer.
        // This should be called by the Renderer within its main render pass,
        // after scene objects have been drawn but before the render pass ends.
        // `commandBuffer` is the active primary command buffer for the current frame.
        virtual void RenderDrawData(VkCommandBuffer commandBuffer);


    private:
        // --- Internal Initialization and Cleanup ---
        void InitImGui(Window& window, VulkanContext& context, VkRenderPass renderPass);
        void CreateImGuiVulkanResources(VulkanContext& context); // Renamed from CreateImGuiResources
        void DestroyImGuiVulkanResources(); // Renamed

        // --- Member Variables ---
        VulkanContext& m_Context; // Store a reference to the Vulkan context

        // Vulkan resources owned by ImGui's Vulkan backend (or managed by us for it)
        VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE; // Dedicated pool for ImGui's descriptor sets

        bool m_FrameBegun = false; // Tracks if ImGui::NewFrame has been called without a corresponding ImGui::Render
        bool m_IsInitialized = false; // Tracks if InitImGui was successful (or skipped)
    };

} // namespace VulkEng
