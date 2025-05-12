#include "Swapchain.h"
#include "VulkanContext.h" // Needs full definition for QuerySwapChainSupport, device, surface
#include "VulkanUtils.h"   // For VK_CHECK and createImageView
#include "core/Log.h"

#include <algorithm> // For std::clamp
#include <limits>    // For std::numeric_limits
#include <stdexcept> // For std::runtime_error

namespace VulkEng {

    Swapchain::Swapchain(VulkanContext& context, uint32_t windowWidth, uint32_t windowHeight, VkSwapchainKHR previousSwapchainHandle /*= VK_NULL_HANDLE*/)
        : m_Context(context),
          m_CurrentWindowWidth(windowWidth),
          m_CurrentWindowHeight(windowHeight),
          m_SwapChain(VK_NULL_HANDLE), // Initialize to null
          m_SwapChainImageFormat(VK_FORMAT_UNDEFINED),
          m_SwapChainExtent({0,0})
    {
        VKENG_INFO("Creating Swapchain (Initial size: {}x{})...", windowWidth, windowHeight);
        Init(previousSwapchainHandle); // Pass previous handle if provided (for complex handoffs)
        VKENG_INFO("Swapchain Created (Image Count: {}, Format: {}, Extent: {}x{}).",
                   m_SwapChainImages.size(), m_SwapChainImageFormat, m_SwapChainExtent.width, m_SwapChainExtent.height);
    }

    Swapchain::~Swapchain() {
        VKENG_INFO("Destroying Swapchain...");
        CleanupCurrentSwapchain();
        VKENG_INFO("Swapchain Destroyed.");
    }

    void Swapchain::Init(VkSwapchainKHR oldSwapchainForRecreation /*= VK_NULL_HANDLE*/) {
        // Device and Surface must be valid to create a swapchain
        if (m_Context.device == VK_NULL_HANDLE || m_Context.surface == VK_NULL_HANDLE) {
            VKENG_ERROR("Swapchain::Init: Vulkan device or surface is NULL. Cannot create swapchain.");
            // This often happens if called by a NullVulkanContext. Allow it to proceed "silently"
            // for Null objects, but a real context should throw or log critical.
            if (m_Context.device != VK_NULL_HANDLE) { // Only throw if device was supposed to be real
                 throw std::runtime_error("Cannot initialize swapchain with null device or surface.");
            }
            return;
        }

        CreateActualSwapchain(oldSwapchainForRecreation);
        RetrieveSwapchainImages();
        CreateSwapchainImageViews();

        // Update VulkanContext with new swapchain details (Renderer or other systems might use these)
        m_Context.imageCount = static_cast<uint32_t>(m_SwapChainImages.size());
        // m_Context.minImageCount is set inside CreateActualSwapchain based on capabilities
    }

    void Swapchain::Recreate(uint32_t newWidth, uint32_t newHeight, VkSwapchainKHR oldSwapchainHandle) {
        VKENG_INFO("Recreating Swapchain (New size: {}x{})...", newWidth, newHeight);
        m_CurrentWindowWidth = newWidth;
        m_CurrentWindowHeight = newHeight;

        // Important: Renderer is responsible for waiting for device idle and
        // cleaning up resources *dependent* on the OLD swapchain's images/views
        // (like framebuffers) BEFORE calling this Recreate method.

        // This class only cleans up its own direct resources (image views, then old swapchain handle).
        CleanupCurrentSwapchain(); // Clean up views and image vector of the *current* m_SwapChain if it exists

        Init(oldSwapchainHandle); // Pass the *actual* old handle for recreation

        VKENG_INFO("Swapchain Recreated (Image Count: {}, Format: {}, Extent: {}x{}).",
                   m_SwapChainImages.size(), m_SwapChainImageFormat, m_SwapChainExtent.width, m_SwapChainExtent.height);
    }

    void Swapchain::CreateActualSwapchain(VkSwapchainKHR oldSwapchainHandle) {
        SwapChainSupportDetails swapChainSupport = m_Context.QuerySwapChainSupport(m_Context.physicalDevice);
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
            // This might be acceptable if surface is VK_NULL_HANDLE (dummy context)
            if (m_Context.surface != VK_NULL_HANDLE) {
                throw std::runtime_error("Device does not adequately support swapchains (no formats/present modes).");
            }
            VKENG_WARN("Swapchain creation skipped due to no surface or inadequate support (likely dummy context).");
            m_SwapChainImageFormat = VK_FORMAT_UNDEFINED; // Ensure it's set to a known invalid state
            m_SwapChainExtent = {0,0};
            return;
        }

        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, m_CurrentWindowWidth, m_CurrentWindowHeight);

        // Determine image count for the swapchain
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // Request one more than minimum for smoother operation
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount; // Don't exceed max allowed
        }
        if (imageCount < swapChainSupport.capabilities.minImageCount) { // Sanity check
            imageCount = swapChainSupport.capabilities.minImageCount;
        }
        m_Context.minImageCount = imageCount; // Store the count we will request

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Context.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // For non-stereoscopic 3D
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Images will be used as color attachments
        // Add VK_IMAGE_USAGE_TRANSFER_DST_BIT if you plan to blit/copy to swapchain images (e.g., for post-processing not done in-place)

        QueueFamilyIndices indices = m_Context.FindQueueFamilies(m_Context.physicalDevice);
        uint32_t queueFamilyIndicesArray[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Images shared between queues
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Faster if queues are the same
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Usually identity
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // No blending with other OS windows
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // Allow clipping of obscured pixels

        createInfo.oldSwapchain = oldSwapchainHandle; // Pass the old handle for efficient resource reuse

        VkResult result = vkCreateSwapchainKHR(m_Context.device, &createInfo, nullptr, &m_SwapChain);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain! Result: " + std::to_string(result));
        }

        // Store the chosen format and extent for later use by other systems
        m_SwapChainImageFormat = surfaceFormat.format;
        m_SwapChainExtent = extent;
    }

    void Swapchain::RetrieveSwapchainImages() {
        if (m_SwapChain == VK_NULL_HANDLE) { // Nothing to retrieve if swapchain wasn't created
            m_SwapChainImages.clear();
            return;
        }
        uint32_t imageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(m_Context.device, m_SwapChain, &imageCount, nullptr));
        m_SwapChainImages.resize(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(m_Context.device, m_SwapChain, &imageCount, m_SwapChainImages.data()));
        VKENG_INFO("Retrieved {} swapchain images.", imageCount);
    }

    void Swapchain::CreateSwapchainImageViews() {
        if (m_SwapChainImages.empty()) { // Nothing to create views for
            m_SwapChainImageViews.clear();
            return;
        }
        m_SwapChainImageViews.resize(m_SwapChainImages.size());
        VKENG_INFO("Creating {} image views for swapchain...", m_SwapChainImages.size());
        for (size_t i = 0; i < m_SwapChainImages.size(); ++i) {
            // Uses the utility function from VulkanUtils.cpp
            m_SwapChainImageViews[i] = Utils::createImageView(
                m_Context.device,
                m_SwapChainImages[i],
                m_SwapChainImageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1 // Mip levels for swapchain images is always 1
            );
            if (m_SwapChainImageViews[i] == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to create image view for swapchain image " + std::to_string(i));
            }
        }
        VKENG_INFO("Swapchain Image Views Created.");
    }

    void Swapchain::CleanupCurrentSwapchain() {
        // Image views must be destroyed before the swapchain itself
        for (auto imageView : m_SwapChainImageViews) {
            if (imageView != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
                vkDestroyImageView(m_Context.device, imageView, nullptr);
            }
        }
        m_SwapChainImageViews.clear();

        if (m_SwapChain != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_Context.device, m_SwapChain, nullptr);
            m_SwapChain = VK_NULL_HANDLE;
        }
        m_SwapChainImages.clear(); // Clear image handles vector
    }


    // --- Helper Functions for Choosing Swapchain Properties ---

    VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // Prefer B8G8R8A8_SRGB with SRGB non-linear color space for best color accuracy
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                VKENG_INFO("Chosen Swapchain Surface Format: B8G8R8A8_SRGB, ColorSpace: SRGB_NONLINEAR_KHR");
                return availableFormat;
            }
        }
        // Fallback to the first format available if the preferred one is not found
        VKENG_WARN("Preferred SRGB surface format not found, using first available: Format {}, ColorSpace {}",
                   availableFormats[0].format, availableFormats[0].colorSpace);
        return availableFormats[0];
    }

    VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        // Prefer Mailbox for lowest latency without tearing (triple buffering effect)
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                VKENG_INFO("Chosen Swapchain Present Mode: MAILBOX_KHR");
                return availablePresentMode;
            }
        }
        // VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available by the spec (standard V-Sync)
        VKENG_INFO("MAILBOX_KHR present mode not found, using FIFO_KHR (V-Sync).");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t windowWidth, uint32_t windowHeight) {
        // If currentExtent has a fixed value, we must use it.
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            VKENG_INFO("Using fixed swapchain extent from surface capabilities: {}x{}",
                       capabilities.currentExtent.width, capabilities.currentExtent.height);
            return capabilities.currentExtent;
        } else {
            // Otherwise, we can choose an extent that best matches the window size,
            // clamped within the min/max extents allowed by the surface.
            VkExtent2D actualExtent = {windowWidth, windowHeight};

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            VKENG_INFO("Chosen clamped swapchain extent: {}x{}", actualExtent.width, actualExtent.height);
            return actualExtent;
        }
    }

} // namespace VulkEng









// 12 May 19:13 | ))
// #include "Swapchain.h"
// #include "VulkanContext.h" // Needs full definition for QuerySwapChainSupport
// #include "VulkanUtils.h"   // For VK_CHECK
// #include "core/Log.h"
// #include <algorithm> // For std::clamp
// #include <limits>    // For std::numeric_limits
// 
// namespace VulkEng {
// 
//     Swapchain::Swapchain(VulkanContext& context, uint32_t windowWidth, uint32_t windowHeight, std::shared_ptr<Swapchain> previous)
//         : m_Context(context), m_WindowWidth(windowWidth), m_WindowHeight(windowHeight), m_OldSwapchain(previous)
//     {
//         VKENG_INFO("Creating Swapchain ({}x{})...", windowWidth, windowHeight);
//         Init();
//         VKENG_INFO("Swapchain Created (Image Count: {}).", m_SwapChainImages.size());
//     }
// 
//     Swapchain::~Swapchain() {
//         VKENG_INFO("Destroying Swapchain...");
//         Cleanup(); // Cleanup current swapchain
//         CleanupOldSwapchain(); // Ensure old one is cleaned up if not already
//         VKENG_INFO("Swapchain Destroyed.");
//     }
// 
//     void Swapchain::Init(bool isRecreation) {
//         if (!isRecreation) { // If it's not a recreation, clear any old swapchain first
//             CleanupOldSwapchain();
//         }
//         CreateActualSwapchain();
//         RetrieveImages();
//         CreateImageViews();
// 
//         // Update context with new swapchain details (Renderer or other systems might use these)
//         m_Context.imageCount = static_cast<uint32_t>(m_SwapChainImages.size());
//         // m_Context.minImageCount can be set based on capabilities.minImageCount used during creation
//     }
// 
//     void Swapchain::Recreate(uint32_t newWidth, uint32_t newHeight) {
//         VKENG_INFO("Recreating Swapchain ({}x{})...", newWidth, newHeight);
//         m_WindowWidth = newWidth;
//         m_WindowHeight = newHeight;
// 
//         // The current swapchain becomes the old one for handoff
//         if (m_SwapChain != VK_NULL_HANDLE) { // Only if a valid swapchain exists
//             if (m_OldSwapchain) { // If there's already an old one, clean it first
//                 m_OldSwapchain->Cleanup();
//             }
//             // This looks a bit complex. Simpler: just pass m_SwapChain handle to CreateActualSwapchain.
//             // For now, we'll assume m_OldSwapchain handles the old one being passed.
//             // Let's refine this: the `previous` in constructor is for handoff.
//             // During Recreate, the *current* swapchain is what needs to be passed as old.
//             // So, m_OldSwapchain should probably be a raw handle managed internally.
// 
//             // For now, we'll pass the current m_SwapChain directly. The old logic was a bit off.
//             // The Renderer will call CleanupSwapchainDependents before calling this,
//             // so image views of the *current* swapchain are already destroyed.
//         }
// 
// 
//         VkSwapchainKHR oldHandleForRecreation = m_SwapChain;
//         m_SwapChain = VK_NULL_HANDLE; // Null out current before creating new one with old handle
// 
//         // Destroy image views of the *current* (soon to be old) swapchain
//         // This is typically done by Renderer *before* calling Recreate
//         for (auto imageView : m_SwapChainImageViews) {
//             vkDestroyImageView(m_Context.device, imageView, nullptr);
//         }
//         m_SwapChainImageViews.clear();
// 
// 
//         Init(true); // Pass true to indicate it's a recreation
// 
//         // Now destroy the actual old swapchain handle if it was valid
//         if (oldHandleForRecreation != VK_NULL_HANDLE) {
//             vkDestroySwapchainKHR(m_Context.device, oldHandleForRecreation, nullptr);
//             VKENG_INFO("Old swapchain handle (after recreation) destroyed.");
//         }
// 
//         VKENG_INFO("Swapchain Recreated.");
//     }
// 
// 
//     void Swapchain::CreateActualSwapchain() {
//         SwapChainSupportDetails swapChainSupport = m_Context.QuerySwapChainSupport(m_Context.physicalDevice);
//         if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
//             throw std::runtime_error("Device does not adequately support swapchains (no formats/present modes).");
//         }
// 
//         VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
//         VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
//         VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, m_WindowWidth, m_WindowHeight);
// 
//         uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
//         if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
//             imageCount = swapChainSupport.capabilities.maxImageCount;
//         }
//         m_Context.minImageCount = imageCount; // Store the count we request
// 
//         VkSwapchainCreateInfoKHR createInfo{};
//         createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
//         createInfo.surface = m_Context.surface;
//         createInfo.minImageCount = imageCount;
//         createInfo.imageFormat = surfaceFormat.format;
//         createInfo.imageColorSpace = surfaceFormat.colorSpace;
//         createInfo.imageExtent = extent;
//         createInfo.imageArrayLayers = 1;
//         createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
// 
//         QueueFamilyIndices indices = m_Context.FindQueueFamilies(m_Context.physicalDevice);
//         uint32_t queueFamilyIndicesArray[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
// 
//         if (indices.graphicsFamily != indices.presentFamily) {
//             createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
//             createInfo.queueFamilyIndexCount = 2;
//             createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
//         } else {
//             createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         }
// 
//         createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
//         createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
//         createInfo.presentMode = presentMode;
//         createInfo.clipped = VK_TRUE;
// 
//         // Pass the handle of the previous swapchain if this is a recreation
//         // Note: m_OldSwapchain is a shared_ptr, its raw handle needs to be obtained if it exists.
//         // However, for Recreate, the 'oldSwapchain' is the one just replaced.
//         // For initial creation, m_OldSwapchain might be from a previous app run or editor state.
//         // The parameter to vkCreateSwapchainKHR is the *actual VkSwapchainKHR handle*.
//         // Let's simplify: Recreate handles passing the old handle explicitly.
//         // For initial creation, createInfo.oldSwapchain = m_OldSwapchain ? m_OldSwapchain->GetSwapchain() : VK_NULL_HANDLE;
//         // This assumes m_OldSwapchain is for more complex handoff scenarios.
//         // For simple recreation, the old handle is passed by Recreate().
//         // The constructor's 'previous' param is for advanced scenarios.
//         // This logic needs careful review for complex scenarios (e.g. editor play-in-editor).
//         // For now: if constructor's `previous` is set, use it. Recreate manages its own "old".
//         createInfo.oldSwapchain = (m_OldSwapchain && m_OldSwapchain->m_SwapChain != VK_NULL_HANDLE) ? m_OldSwapchain->m_SwapChain : VK_NULL_HANDLE;
// 
// 
//         VK_CHECK(vkCreateSwapchainKHR(m_Context.device, &createInfo, nullptr, &m_SwapChain));
// 
//         m_SwapChainImageFormat = surfaceFormat.format;
//         m_SwapChainExtent = extent;
//     }
// 
//     void Swapchain::RetrieveImages() {
//         uint32_t imageCount = 0; // Use local variable to get actual count
//         vkGetSwapchainImagesKHR(m_Context.device, m_SwapChain, &imageCount, nullptr);
//         m_SwapChainImages.resize(imageCount);
//         vkGetSwapchainImagesKHR(m_Context.device, m_SwapChain, &imageCount, m_SwapChainImages.data());
//         VKENG_INFO("Retrieved {} swapchain images.", imageCount);
//     }
// 
//     void Swapchain::CreateImageViews() {
//         m_SwapChainImageViews.resize(m_SwapChainImages.size());
//         VKENG_INFO("Creating {} image views...", m_SwapChainImages.size());
//         for (size_t i = 0; i < m_SwapChainImages.size(); ++i) {
//             m_SwapChainImageViews[i] = Utils::createImageView(m_Context.device, m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
//         }
//         VKENG_INFO("Image Views Created.");
//     }
// 
//     void Swapchain::Cleanup() {
//         for (auto imageView : m_SwapChainImageViews) {
//             if (imageView != VK_NULL_HANDLE)
//                 vkDestroyImageView(m_Context.device, imageView, nullptr);
//         }
//         m_SwapChainImageViews.clear();
// 
//         if (m_SwapChain != VK_NULL_HANDLE) {
//             vkDestroySwapchainKHR(m_Context.device, m_SwapChain, nullptr);
//             m_SwapChain = VK_NULL_HANDLE;
//         }
//     }
// 
//     void Swapchain::CleanupOldSwapchain() {
//         if (m_OldSwapchain && m_OldSwapchain->m_SwapChain != VK_NULL_HANDLE) {
//             VKENG_INFO("Cleaning up previous (old) swapchain explicitly.");
//             m_OldSwapchain->Cleanup(); // Calls cleanup on the shared_ptr's object
//             m_OldSwapchain.reset();    // Release shared_ptr
//         }
//     }
// 
//     VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
//         for (const auto& availableFormat : availableFormats) {
//             if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
//                 return availableFormat;
//             }
//         }
//         return availableFormats[0]; // Fallback
//     }
// 
//     VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
//         for (const auto& availablePresentMode : availablePresentModes) {
//             if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
//         }
//         // VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available
//         return VK_PRESENT_MODE_FIFO_KHR;
//     }
// 
//     VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t windowWidth, uint32_t windowHeight) {
//         if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
//             return capabilities.currentExtent;
//         } else {
//             VkExtent2D actualExtent = {windowWidth, windowHeight};
//             actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
//             actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
//             return actualExtent;
//         }
//     }
// } // namespace VulkEng
