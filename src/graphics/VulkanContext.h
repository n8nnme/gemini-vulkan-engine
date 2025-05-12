#pragma once

#include "core/Window.h" // Needs Window definition for constructor and surface creation
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional> // For std::optional in QueueFamilyIndices

namespace VulkEng {

    // Structure to hold queue family indices found on the physical device
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        // Optional: Add computeFamily, transferFamily if needed later
        // std::optional<uint32_t> computeFamily;
        // std::optional<uint32_t> transferFamily;

        bool IsComplete() const {
            // For basic rendering, graphics and present are essential
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    // Structure to hold details about swap chain support for a physical device
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{}; // Initialize to default
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class VulkanContext {
    public:
        // Constructor takes a reference to the application's Window and an optional skipInit flag.
        VulkanContext(Window& window, bool skipVulkanInit = false);
        // Virtual destructor for potential inheritance (e.g., DummyVulkanContextSL).
        virtual ~VulkanContext();

        // Prevent copying and assignment.
        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;

        // --- Device Query Methods (made virtual for potential overriding by dummies) ---
        // Finds suitable queue families on the given physical device.
        virtual QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
        // Queries swap chain support details for the given physical device and surface.
        virtual SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

        // --- Helper for Single Time Commands ---
        // These might be better in VulkanUtils or CommandManager, but useful here if context owns a utility pool.
        // For now, assuming CommandManager will provide its own pool for such operations.
        // If VulkanContext were to manage a general-purpose command pool:
        // VkCommandBuffer BeginSingleTimeCommandsHelper(VkCommandPool utilityCommandPool);
        // void EndSingleTimeCommandsHelper(VkCommandPool utilityCommandPool, VkCommandBuffer commandBuffer);


        // --- Public Vulkan Handles and Members ---
        // (Consider making these private with const getters for better encapsulation)
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE; // If validation layers are enabled
        VkSurfaceKHR surface = VK_NULL_HANDLE;                   // Window surface

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE; // Logical device

        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        // Optional: computeQueue, transferQueue

        // Store queue family indices
        uint32_t graphicsQueueFamily = UINT32_MAX; // Use an invalid default
        uint32_t presentQueueFamily = UINT32_MAX;

        // Cached properties and features of the selected physical device
        VkPhysicalDeviceProperties physicalDeviceProperties{};
        VkPhysicalDeviceFeatures physicalDeviceFeatures{};
        // Add VkPhysicalDeviceVulkan11Features, VkPhysicalDeviceVulkan12Features,
        // VkPhysicalDeviceVulkan13Features if specific features are queried and used.

        // --- State Shared with Other Systems (often set by Renderer/Swapchain) ---
        // This is a bit of a "global state" within the context; manage carefully.
        VkRenderPass mainRenderPass = VK_NULL_HANDLE; // Main render pass used by Renderer & UIManager
        uint32_t imageCount = 0;       // Actual number of images in the current swapchain
        uint32_t minImageCount = 0;    // Minimum images required by the swapchain (from capabilities)


    private:
        // Allow DummyVulkanContextSL (for NullServices) to access private members for its "null" construction.
        // Ideally, this wouldn't be needed if using interfaces.
        friend class DummyVulkanContextSL;

        // --- Initialization Steps ---
        void CreateInstance();
        bool CheckValidationLayerSupport(); // Helper for CreateInstance
        std::vector<const char*> GetRequiredInstanceExtensions(); // Helper for CreateInstance
        void SetupDebugMessenger();          // If validation layers are enabled
        void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo); // Helper for debug messenger
        void PickPhysicalDevice();
        bool IsDeviceSuitable(VkPhysicalDevice device); // Helper for PickPhysicalDevice
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device); // Helper for IsDeviceSuitable
        void CreateLogicalDevice();

        // Reference to the application window for surface creation.
        Window& m_Window;
    };

} // namespace VulkEng












// 12 May 19:09 | I am not a human after all
// #pragma once
// 
// #include "core/Window.h" // Needs Window definition
// #include <vulkan/vulkan.h>
// #include <vector>
// #include <optional> // For optional queue family indices
// #include <string>   // For device extensions list
// 
// namespace VulkEng {
// 
//     struct QueueFamilyIndices {
//         std::optional<uint32_t> graphicsFamily;
//         std::optional<uint32_t> presentFamily;
//         // std::optional<uint32_t> computeFamily;
//         // std::optional<uint32_t> transferFamily;
// 
//         bool IsComplete() const {
//             return graphicsFamily.has_value() && presentFamily.has_value();
//         }
//     };
// 
//     struct SwapChainSupportDetails {
//         VkSurfaceCapabilitiesKHR capabilities;
//         std::vector<VkSurfaceFormatKHR> formats;
//         std::vector<VkPresentModeKHR> presentModes;
//     };
// 
//     class VulkanContext {
//     public:
//         VulkanContext(Window& window, bool skipVulkanInit = false);
//         virtual ~VulkanContext();
// 
//         VulkanContext(const VulkanContext&) = delete;
//         VulkanContext& operator=(const VulkanContext&) = delete;
// 
//         virtual QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
//         virtual SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
// 
//         // Helper for single time commands (might be moved to CommandManager or Utils later)
//         VkCommandBuffer BeginSingleTimeCommandsHelper(VkCommandPool pool);
//         void EndSingleTimeCommandsHelper(VkCommandPool pool, VkCommandBuffer commandBuffer);
// 
// 
//         VkInstance instance = VK_NULL_HANDLE;
//         VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
//         VkSurfaceKHR surface = VK_NULL_HANDLE;
//         VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
//         VkDevice device = VK_NULL_HANDLE;
//         VkQueue graphicsQueue = VK_NULL_HANDLE;
//         VkQueue presentQueue = VK_NULL_HANDLE;
//         uint32_t graphicsQueueFamily = UINT32_MAX;
//         uint32_t presentQueueFamily = UINT32_MAX;
// 
//         VkPhysicalDeviceProperties physicalDeviceProperties;
//         VkPhysicalDeviceFeatures physicalDeviceFeatures;
// 
//         VkRenderPass mainRenderPass = VK_NULL_HANDLE;
//         uint32_t imageCount = 0;
//         uint32_t minImageCount = 0;
// 
//     private:
//         friend class DummyVulkanContextSL; // For NullServices
//         void CreateInstance();
//         bool CheckValidationLayerSupport();
//         std::vector<const char*> GetRequiredInstanceExtensions();
//         void SetupDebugMessenger();
//         void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
//         void PickPhysicalDevice();
//         bool IsDeviceSuitable(VkPhysicalDevice device);
//         bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
//         void CreateLogicalDevice();
// 
//         Window& m_Window;
//     };
// }
