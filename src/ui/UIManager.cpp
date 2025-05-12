#include "UIManager.h"
#include "core/Log.h"
#include "graphics/VulkanContext.h" // For Vulkan handles
#include "graphics/VulkanUtils.h"   // For VK_CHECK (if used, else handle VkResult directly)
#include "core/Window.h"           // For GLFWwindow handle

// --- ImGui Backend Headers ---
// These must be included after imgui.h (which is typically included by UIManager.h or directly here)
#include <imgui.h> // Should be included by UIManager.h or here
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// For font upload (single time command)
// #include "graphics/CommandManager.h" // Or use helpers from VulkanContext/Utils

namespace VulkEng {

    // Helper function to check ImGui Vulkan results (can be part of VulkanUtils too)
    static void ImGui_CheckVkResult(VkResult err) {
        if (err == 0) return;
        VKENG_ERROR("[ImGui Vulkan] Error: VkResult = {}", static_cast<int>(err));
        if (err < 0) {
            // Consider throwing or a more graceful error handling
            throw std::runtime_error("ImGui Vulkan backend error: " + std::to_string(err));
        }
    }


    UIManager::UIManager(Window& window, VulkanContext& context, VkRenderPass renderPass, bool skipInit /*= false*/)
        : m_Context(context), m_ImGuiDescriptorPool(VK_NULL_HANDLE), m_FrameBegun(false), m_IsInitialized(false)
    {
        if (skipInit) {
            VKENG_WARN("UIManager: Skipping ImGui Initialization for Dummy/Null instance!");
            return; // Exit early
        }

        if (m_Context.device == VK_NULL_HANDLE || m_Context.instance == VK_NULL_HANDLE || m_Context.physicalDevice == VK_NULL_HANDLE) {
            VKENG_ERROR("UIManager: Cannot initialize with invalid Vulkan context (null device/instance/physicalDevice).");
            throw std::runtime_error("Invalid Vulkan context for UIManager initialization.");
        }
        if (renderPass == VK_NULL_HANDLE) {
            VKENG_ERROR("UIManager: Cannot initialize with a NULL render pass for ImGui!");
            throw std::runtime_error("Null render pass for UIManager initialization.");
        }

        InitImGui(window, context, renderPass);
        m_IsInitialized = true; // Mark as initialized if InitImGui doesn't throw
    }

    UIManager::~UIManager() {
        VKENG_INFO("UIManager: Destroying...");

        if (m_IsInitialized) { // Only shutdown if initialized
            // Ensure GPU is idle before destroying ImGui Vulkan resources.
            // This might be redundant if Renderer::WaitForDeviceIdle is called before UIManager destruction.
            if (m_Context.device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(m_Context.device);
            }

            // Check if ImGui context exists before shutdown.
            // ImGui::GetCurrentContext() returns nullptr if no context is active.
            if (ImGui::GetCurrentContext() != nullptr && ImGui::GetCurrentContext()->Initialized) {
                ImGui_ImplVulkan_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
                 VKENG_INFO("UIManager: ImGui context and backends shut down.");
            }
        }

        DestroyImGuiVulkanResources(); // Destroy our manually created descriptor pool
        VKENG_INFO("UIManager: Destroyed.");
    }

    void UIManager::InitImGui(Window& window, VulkanContext& context, VkRenderPass renderPass) {
        VKENG_INFO("UIManager: Initializing ImGui context and backends...");

        // 1. Create ImGui Context & Setup IO
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
        //io.ConfigViewportsNoAutoMerge = true;
        //io.ConfigViewportsNoTaskBarIcon = true;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark(); // or ImGui::StyleColorsLight(); or ImGui::StyleColorsClassic();

        // When viewports are enabled, tweak WindowRounding/WindowBg so platform windows can look like regular ones.
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // 2. Initialize Platform Backend (GLFW)
        if (!ImGui_ImplGlfw_InitForVulkan(window.GetGLFWwindow(), true)) { // true: install callbacks
            throw std::runtime_error("Failed to initialize ImGui GLFW backend!");
        }
        VKENG_INFO("UIManager: ImGui GLFW backend initialized.");

        // 3. Create Vulkan Resources for ImGui (like its own descriptor pool)
        CreateImGuiVulkanResources(context);
        if (m_ImGuiDescriptorPool == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create ImGui descriptor pool!");
        }

        // 4. Initialize Renderer Backend (Vulkan)
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = context.instance;
        init_info.PhysicalDevice = context.physicalDevice;
        init_info.Device = context.device;
        init_info.QueueFamily = context.graphicsQueueFamily; // Assumes graphics queue can do transfers
        init_info.Queue = context.graphicsQueue;
        init_info.PipelineCache = VK_NULL_HANDLE; // Optional
        init_info.DescriptorPool = m_ImGuiDescriptorPool;
        init_info.Subpass = 0; // ImGui will render in the first subpass of the provided renderPass
        // Get these from VulkanContext (which should be updated by Swapchain)
        init_info.MinImageCount = context.minImageCount;
        init_info.ImageCount = context.imageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA for ImGui pass, or match main pass
        init_info.Allocator = nullptr; // Use default Vulkan allocator
        init_info.CheckVkResultFn = ImGui_CheckVkResult; // Our error checking function

        if (!ImGui_ImplVulkan_Init(&init_info, renderPass)) {
            ImGui_ImplGlfw_Shutdown(); // Cleanup GLFW backend if Vulkan fails
            ImGui::DestroyContext();
            DestroyImGuiVulkanResources();
            throw std::runtime_error("Failed to initialize ImGui Vulkan backend!");
        }
        VKENG_INFO("UIManager: ImGui Vulkan backend initialized.");


        // 5. Upload Fonts
        // This needs a command buffer. Use a utility function for single-time commands.
        // Assuming VulkanContext has such helpers, or CommandManager does.
        // For now, using VulkanContext's helpers with a temporary pool.
        VKENG_INFO("UIManager: Uploading ImGui fonts...");
        VkCommandPool tempCommandPool = VK_NULL_HANDLE; // Create a temporary pool for font upload
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = context.graphicsQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // For short-lived command buffers
        VK_CHECK(vkCreateCommandPool(context.device, &poolInfo, nullptr, &tempCommandPool));

        VkCommandBuffer commandBuffer = Utils::BeginSingleTimeCommands(context.device, tempCommandPool);
        if (commandBuffer != VK_NULL_HANDLE) {
            if (!ImGui_ImplVulkan_CreateFontsTexture(commandBuffer)) {
                Utils::EndSingleTimeCommands(context.device, tempCommandPool, context.graphicsQueue, commandBuffer);
                vkDestroyCommandPool(context.device, tempCommandPool, nullptr);
                throw std::runtime_error("Failed to create ImGui font textures!");
            }
            Utils::EndSingleTimeCommands(context.device, tempCommandPool, context.graphicsQueue, commandBuffer);
            ImGui_ImplVulkan_DestroyFontUploadObjects(); // Clean up staging buffers used by ImGui
            VKENG_INFO("UIManager: ImGui fonts uploaded and staging resources destroyed.");
        } else {
             vkDestroyCommandPool(context.device, tempCommandPool, nullptr);
             throw std::runtime_error("Failed to begin single time command buffer for ImGui font upload.");
        }
        vkDestroyCommandPool(context.device, tempCommandPool, nullptr); // Destroy temporary pool

        VKENG_INFO("UIManager: ImGui fully initialized.");
    }

    void UIManager::CreateImGuiVulkanResources(VulkanContext& context) {
        // Create a descriptor pool for ImGui's own needs.
        // Sizes copied from ImGui Vulkan example, adjust if needed.
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
        pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;

        VkResult result = vkCreateDescriptorPool(context.device, &pool_info, nullptr, &m_ImGuiDescriptorPool);
        if (result != VK_SUCCESS) {
            m_ImGuiDescriptorPool = VK_NULL_HANDLE; // Ensure it's null on failure
            VKENG_ERROR("UIManager: Failed to create ImGui descriptor pool! Error: {}", result);
        } else {
            VKENG_INFO("UIManager: ImGui Descriptor Pool created.");
        }
    }

    void UIManager::DestroyImGuiVulkanResources() {
        if (m_ImGuiDescriptorPool != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_Context.device, m_ImGuiDescriptorPool, nullptr);
            m_ImGuiDescriptorPool = VK_NULL_HANDLE;
            VKENG_INFO("UIManager: ImGui Descriptor Pool destroyed.");
        }
    }

    void UIManager::BeginUIRender() {
        if (!m_IsInitialized) return; // Do nothing if not initialized
        ImGui_ImplVulkan_NewFrame(); // Must be called before ImGui_ImplGlfw_NewFrame
        ImGui_ImplGlfw_NewFrame();   // Handles input for ImGui
        ImGui::NewFrame();           // Starts a new ImGui frame for UI definition
        m_FrameBegun = true;
    }

    void UIManager::EndUIRender() {
        if (!m_IsInitialized || !m_FrameBegun) {
            if(m_IsInitialized && !m_FrameBegun) VKENG_WARN("UIManager::EndUIRender called without BeginUIRender!");
            return;
        }
        ImGui::Render(); // Finalizes ImGui's internal draw data list.
        m_FrameBegun = false;
    }

    void UIManager::RenderDrawData(VkCommandBuffer commandBuffer) {
        if (!m_IsInitialized) return;

        ImDrawData* draw_data = ImGui::GetDrawData();
        // Check if draw_data is valid and has commands
        if (!draw_data || draw_data->CmdListsCount == 0) {
            return; // Nothing to render
        }

        // Record ImGui draw commands into the provided command buffer.
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);

        // Update and Render additional Platform Windows (if ImGuiConfigFlags_ViewportsEnable is set)
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault(); // This uses its own Vulkan contexts for other windows
        }
    }

} // namespace VulkEng
