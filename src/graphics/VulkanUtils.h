#pragma once

#include "core/Log.h" // For logging macros used in VK_CHECK
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <stdexcept> // For std::runtime_error in VK_CHECK

// --- Vulkan Result Checking Macro ---
// Throws a std::runtime_error if a Vulkan call fails.
// Logs the error details including file and line number.
#define VK_CHECK(vk_result)                                                 \
    do {                                                                    \
        VkResult res = (vk_result);                                         \
        if (res != VK_SUCCESS) {                                            \
            std::string error_message = "Vulkan Error: " +                  \
                                       VulkEng::Utils::VkResultToString(res) + \
                                       " in file " + __FILE__ +             \
                                       " at line " + std::to_string(__LINE__); \
            VKENG_CRITICAL(error_message);                                   \
            throw std::runtime_error(error_message);                        \
        }                                                                   \
    } while (0)


namespace VulkEng {
namespace Utils {

    // --- Helper Function Declarations ---

    // Converts VkResult to a human-readable string for error messages.
    std::string VkResultToString(VkResult result);

    // Finds a suitable memory type index on the physical device.
    // - typeFilter: Bitmask of memory types allowed (from VkMemoryRequirements::memoryTypeBits).
    // - properties: Required VkMemoryPropertyFlags (e.g., HOST_VISIBLE, DEVICE_LOCAL).
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Finds a supported format from a list of candidates for a given tiling and features.
    // - candidates: Vector of VkFormat candidates to check.
    // - tiling: VK_IMAGE_TILING_OPTIMAL or VK_IMAGE_TILING_LINEAR.
    // - features: Required VkFormatFeatureFlags (e.g., for depth, color attachment, sampling).
    VkFormat findSupportedFormat(
        VkPhysicalDevice physicalDevice,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    );

    // Checks if a given VkFormat has a stencil component.
    bool hasStencilComponent(VkFormat format);

    // Creates a VkImage and allocates/binds VkDeviceMemory for it.
    // Does NOT handle layout transitions.
    void createImage(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples, // For MSAA
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, // Memory properties for allocation
        VkImage& outImage,                // Output image handle
        VkDeviceMemory& outImageMemory    // Output memory handle
    );

    // Creates a VkImageView for a given VkImage.
    VkImageView createImageView(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags, // e.g., VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT
        uint32_t mipLevels
    );

    // --- Command Buffer Utility Functions ---
    // (These require VkDevice, VkCommandPool, and VkQueue, so they are good utilities)

    // Begins a single-time command buffer from the given pool.
    // The caller is responsible for ending and submitting it using EndSingleTimeCommands.
    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

    // Ends, submits, waits for, and frees a single-time command buffer.
    void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer);


    // --- Resource Transition and Copy Utility Functions ---

    // Transitions the layout of a VkImage using a pipeline barrier.
    void TransitionImageLayout(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkImage image,
        VkFormat format, // Needed for stencil aspect if applicable
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels = 1,    // Number of mip levels in the image
        uint32_t baseMipLevel = 0, // Base mip level to apply transition to
        uint32_t layerCount = 1,   // For image arrays
        uint32_t baseArrayLayer = 0
    );

    // Copies data from one VkBuffer to another.
    void CopyBuffer(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size,
        VkDeviceSize srcOffset = 0, // Optional source offset
        VkDeviceSize dstOffset = 0  // Optional destination offset
    );

    // Copies data from a VkBuffer to a VkImage.
    // Assumes the image is already in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
    void CopyBufferToImage(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer buffer,
        VkImage image,
        uint32_t width,
        uint32_t height,
        uint32_t layerCount = 1,   // For image arrays
        uint32_t baseArrayLayer = 0
    );

    // Generates mipmaps for an image using vkCmdBlitImage.
    // Image must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL initially for mip 0 copy.
    // Image usage must include VK_IMAGE_USAGE_TRANSFER_SRC_BIT and VK_IMAGE_USAGE_TRANSFER_DST_BIT.
    // Final layout of all mip levels will be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    void GenerateMipmaps(
        VkDevice device,
        VkPhysicalDevice physicalDevice, // Needed to check blit support
        VkCommandPool commandPool,
        VkQueue queue,
        VkImage image,
        VkFormat imageFormat,
        int32_t texWidth,
        int32_t texHeight,
        uint32_t mipLevels
    );


} // namespace Utils
} // namespace VulkEng
