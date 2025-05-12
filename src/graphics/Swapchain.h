#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory> // For std::shared_ptr (used for oldSwapchain in more complex scenarios)
#include <cstdint> // For uint32_t

namespace VulkEng {

    // Forward declaration
    class VulkanContext;

    // SwapChainSupportDetails is now defined in VulkanContext.h.
    // If it were specific to Swapchain, it would be here.

    class Swapchain {
    public:
        // Constructor:
        // - context: Reference to the initialized VulkanContext.
        // - windowWidth/Height: Initial dimensions for the swapchain.
        // - previousSwapchainHandle: Optional handle to an old swapchain for smoother recreation.
        Swapchain(VulkanContext& context, uint32_t windowWidth, uint32_t windowHeight, VkSwapchainKHR previousSwapchainHandle = VK_NULL_HANDLE);
        ~Swapchain();

        // Prevent copying and assignment.
        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;

        // Recreates the swapchain, typically called on window resize.
        // Takes new dimensions and the handle of the *current* (soon to be old) swapchain.
        void Recreate(uint32_t newWidth, uint32_t newHeight, VkSwapchainKHR oldSwapchainHandle);

        // --- Getters for Swapchain Properties ---
        VkSwapchainKHR GetVKSwapchain() const { return m_SwapChain; } // Changed name to avoid conflict
        VkFormat GetImageFormat() const { return m_SwapChainImageFormat; }
        VkExtent2D GetExtent() const { return m_SwapChainExtent; }
        uint32_t GetImageCount() const { return static_cast<uint32_t>(m_SwapChainImages.size()); }

        const std::vector<VkImage>& GetImages() const { return m_SwapChainImages; }
        const std::vector<VkImageView>& GetImageViews() const { return m_SwapChainImageViews; }

    private:
        // Initializes or reinitializes the swapchain.
        // `oldSwapchainForRecreation` is the actual handle of the previous swapchain being replaced.
        void Init(VkSwapchainKHR oldSwapchainForRecreation = VK_NULL_HANDLE);

        // Core creation steps
        void CreateActualSwapchain(VkSwapchainKHR oldSwapchainHandle); // Renamed for clarity
        void RetrieveSwapchainImages(); // Renamed for clarity
        void CreateSwapchainImageViews(); // Renamed for clarity

        // Cleanup resources owned by this swapchain instance.
        void CleanupCurrentSwapchain();


        // Helper functions for choosing optimal swapchain settings.
        VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t windowWidth, uint32_t windowHeight);

        // --- Members ---
        VulkanContext& m_Context; // Reference to the Vulkan context
        uint32_t m_CurrentWindowWidth;  // Store current dimensions used for creation
        uint32_t m_CurrentWindowHeight;

        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
        std::vector<VkImage> m_SwapChainImages;         // Handles to the swapchain images
        std::vector<VkImageView> m_SwapChainImageViews; // Image views for each swapchain image
        VkFormat m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_SwapChainExtent = {0,0};
    };

} // namespace VulkEng




// 12 May 19:13 | okay
// #pragma once
// 
// #include <vulkan/vulkan.h>
// #include <vector>
// #include <memory> // For std::shared_ptr
// 
// namespace VulkEng {
// 
//     class VulkanContext; // Forward declaration
// 
//     // Details about swapchain support (moved from VulkanContext.h for clarity, or keep there)
//     // struct SwapChainSupportDetails {
//     //     VkSurfaceCapabilitiesKHR capabilities;
//     //     std::vector<VkSurfaceFormatKHR> formats;
//     //     std::vector<VkPresentModeKHR> presentModes;
//     // };
// 
//     class Swapchain {
//     public:
//         // Previous swapchain is passed for smoother recreation (optional)
//         Swapchain(VulkanContext& context, uint32_t windowWidth, uint32_t windowHeight, std::shared_ptr<Swapchain> previous = nullptr);
//         ~Swapchain();
// 
//         Swapchain(const Swapchain&) = delete;
//         Swapchain& operator=(const Swapchain&) = delete;
// 
//         void Recreate(uint32_t newWidth, uint32_t newHeight);
// 
//         VkSwapchainKHR GetSwapchain() const { return m_SwapChain; }
//         VkFormat GetImageFormat() const { return m_SwapChainImageFormat; }
//         VkExtent2D GetExtent() const { return m_SwapChainExtent; }
//         uint32_t GetImageCount() const { return static_cast<uint32_t>(m_SwapChainImages.size()); }
//         const std::vector<VkImage>& GetImages() const { return m_SwapChainImages; }
//         const std::vector<VkImageView>& GetImageViews() const { return m_SwapChainImageViews; }
// 
//     private:
//         void Init(bool isRecreation = false); // Combined creation logic
//         void CreateActualSwapchain();
//         void RetrieveImages();
//         void CreateImageViews();
//         void Cleanup();        // Destroys swapchain and image views
//         void CleanupOldSwapchain(); // Helper for recreation
// 
//         // Helper functions for choosing swapchain properties
//         VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
//         VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
//         VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t windowWidth, uint32_t windowHeight);
// 
//         VulkanContext& m_Context;
//         uint32_t m_WindowWidth;
//         uint32_t m_WindowHeight;
// 
//         VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
//         std::vector<VkImage> m_SwapChainImages;
//         std::vector<VkImageView> m_SwapChainImageViews;
//         VkFormat m_SwapChainImageFormat;
//         VkExtent2D m_SwapChainExtent;
// 
//         std::shared_ptr<Swapchain> m_OldSwapchain; // For recreation handoff
//     };
// 
// } // namespace VulkEng
