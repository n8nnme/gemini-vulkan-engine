#include "VulkanUtils.h"
#include "core/Log.h" // For logging within utilities (e.g., warnings)

#include <vector>
#include <stdexcept> // For std::runtime_error, std::invalid_argument
#include <cmath>     // For std::floor, std::log2, std::max in GenerateMipmaps

namespace VulkEng {
namespace Utils {

    // --- VkResult to String Implementation ---
    std::string VkResultToString(VkResult result) {
        switch (result) {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
            case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
            case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
            case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
            case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
            case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
            case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
            case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
            default: return "Unknown VkResult Value: " + std::to_string(result);
        }
    }


    // --- Memory Type Finder Implementation ---
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            // Check if the i-th bit is set in typeFilter (meaning this memory type is allowed)
            // AND check if this memory type has ALL the required property flags.
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i; // Found a suitable memory type
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }

    // --- Format Finder Implementation ---
    VkFormat findSupportedFormat(
        VkPhysicalDevice physicalDevice,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("Failed to find supported format!");
    }

    // --- Stencil Component Check Implementation ---
    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    // --- createImage Implementation ---
    void createImage(
        VkDevice device, VkPhysicalDevice physicalDevice,
        uint32_t width, uint32_t height, uint32_t mipLevels,
        VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& outImage, VkDeviceMemory& outImageMemory)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = numSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Can be concurrent if needed

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &outImage));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, outImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &outImageMemory));
        VK_CHECK(vkBindImageMemory(device, outImage, outImageMemory, 0));
    }

    // --- createImageView Implementation ---
    VkImageView createImageView(
        VkDevice device, VkImage image, VkFormat format,
        VkImageAspectFlags aspectFlags, uint32_t mipLevels)
    {
        if (image == VK_NULL_HANDLE) {
             VKENG_ERROR("Utils::createImageView: Attempted to create view for a NULL image.");
             return VK_NULL_HANDLE; // Or throw
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // Assuming 2D, could be param for 1D/3D/Cube
        viewInfo.format = format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1; // Assuming single layer image

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageView));
        return imageView;
    }

    // --- Command Buffer Utility Implementations ---
    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
        if (device == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE) {
            VKENG_ERROR("BeginSingleTimeCommands: Invalid device or command pool.");
            throw std::runtime_error("Null device or command pool for single time commands.");
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
        return commandBuffer;
    }

    void EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer) {
        if (device == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE) {
            VKENG_ERROR("EndSingleTimeCommands: Invalid device, pool, queue, or command buffer.");
            // Attempt to free command buffer if possible to avoid leak, though state is bad.
            if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE && commandBuffer != VK_NULL_HANDLE) {
                 vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            }
            throw std::runtime_error("Null arguments for ending single time commands.");
        }
        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkFence fence;
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
        VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX)); // Wait indefinitely

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    // --- Resource Transition and Copy Implementations ---
    void TransitionImageLayout(
        VkDevice device, VkCommandPool commandPool, VkQueue queue,
        VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        uint32_t mipLevels /*= 1*/, uint32_t baseMipLevel /*= 0*/,
        uint32_t layerCount /*= 1*/, uint32_t baseArrayLayer /*= 0*/)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Not transferring ownership
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        // Define aspect mask based on new layout or format
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
            oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) { // Also check oldLayout if transitioning *from* depth
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = mipLevels; // Number of levels to transition
        barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
        barrier.subresourceRange.layerCount = layerCount;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        // Determine access masks and pipeline stages based on layout transition type
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Or vertex if sampled there
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            // Transition for using as a source for blits (mipmapping)
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // After being a blit source, transition to shader read
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition in Utils::TransitionImageLayout!");
        }

        vkCmdPipelineBarrier(commandBuffer,
            sourceStage, destinationStage,
            0, // Dependency flags
            0, nullptr, // Memory barriers
            0, nullptr, // Buffer memory barriers
            1, &barrier  // Image memory barriers
        );

        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    void CopyBuffer(
        VkDevice device, VkCommandPool commandPool, VkQueue queue,
        VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,
        VkDeviceSize srcOffset /*= 0*/, VkDeviceSize dstOffset /*= 0*/)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, ©Region);
        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    void CopyBufferToImage(
        VkDevice device, VkCommandPool commandPool, VkQueue queue,
        VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
        uint32_t layerCount /*= 1*/, uint32_t baseArrayLayer /*= 0*/)
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);
        VkBufferImageCopy region{};
        region.bufferOffset = 0;      // Tightly packed pixel data in buffer
        region.bufferRowLength = 0;   // 0 means texels are tightly packed based on imageExtent.width
        region.bufferImageHeight = 0; // 0 means texels are tightly packed based on imageExtent.height

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0; // Copy to base mip level
        region.imageSubresource.baseArrayLayer = baseArrayLayer;
        region.imageSubresource.layerCount = layerCount;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1}; // Depth is 1 for 2D images

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // Image must be in this layout for copy
            1, // regionCount
            ®ion
        );
        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
    }

    void GenerateMipmaps(
        VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
        VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
    {
        if (mipLevels <= 1) return; // No mips to generate

        // Check if format supports linear blitting (for better quality mipmaps)
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            VKENG_WARN("Mipmap generation: Texture format {} does not support linear blit filtering! Mipmaps may look worse.", imageFormat);
            // Could fall back to VK_FILTER_NEAREST, but linear is generally preferred.
        }
         if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
             !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
            VKENG_ERROR("Mipmap generation: Texture format {} does not support blit operations (SRC or DST). Cannot generate mipmaps.", imageFormat);
            return; // Cannot proceed
        }


        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1; // Barrier applies to one mip level at a time

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            // Transition mip level (i-1) to TRANSFER_SRC_OPTIMAL
            // This mip level was previously either undefined (for mip 0 after initial copy)
            // or TRANSFER_DST_OPTIMAL (if it was a target of a previous blit)
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // Mip 0 is already DST_OPT from initial copy
                                                                     // Subsequent mips (i-1) were DST_OPT from previous blit
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &barrier);

            // Define the blit operation from level (i-1) to level (i)
            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};

            // Perform the blit
            // The destination subresource (mip level i) must be in TRANSFER_DST_OPTIMAL layout
            // Before this loop, all mips except level 0 are UNDEFINED.
            // We need to transition mip level 'i' to TRANSFER_DST before blitting *to* it.
            VkImageMemoryBarrier dstBarrier = barrier; // Copy previous barrier settings
            dstBarrier.subresourceRange.baseMipLevel = i; // Target current mip level 'i'
            dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            dstBarrier.srcAccessMask = 0; // No prior access to wait for on undefined layout
            dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &dstBarrier);


            vkCmdBlitImage(commandBuffer,
                           image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // Source mip
                           image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // Destination mip
                           1, &blit,
                           VK_FILTER_LINEAR); // Use linear for smoother mipmaps

            // Update dimensions for the next level's source
            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        // After blitting all levels, transition all mip levels to SHADER_READ_ONLY_OPTIMAL
        // (Except mip 0 which might have been transitioned earlier if only 1 mip level)
        // Simpler: Transition the whole image resource (all mip levels) in one go.
        // The previous TRANSFER_SRC or TRANSFER_DST layouts need to be accounted for.
        // It's often safer to transition each mip level individually as its final step,
        // or ensure all are in a compatible source layout for a bulk transition.

        // For simplicity, let's assume all source mips (0 to mipLevels-2) were left in TRANSFER_SRC
        // and the last target mip (mipLevels-1) was left in TRANSFER_DST.
        // We'll transition all of them to SHADER_READ_ONLY.
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels; // All mip levels
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // This might not be true for all mips
                                                                // The very last mip written is in TRANSFER_DST_OPTIMAL
                                                                // This single barrier needs to handle both.
                                                                // A loop to transition each mip individually might be more robust
                                                                // or two separate barriers.
        // Let's do two: one for SRC->SHADER_READ, one for DST->SHADER_READ
        // Mips 0 to N-2 were SRC, Mip N-1 was DST

        if (mipLevels > 1) { // If mips were generated
            // Transition mips that were blit *from* (i-1 levels)
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = mipLevels - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &barrier);

            // Transition the last mip level that was blit *to*
            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &barrier);
        } else { // Only one mip level, was already made TRANSFER_SRC_OPTIMAL for consistency
             barrier.subresourceRange.baseMipLevel = 0;
             barrier.subresourceRange.levelCount = 1;
             barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // If LoadTexture copied directly to it and didn't transition to SRC
                                                                       // In AssetManager, mip 0 is copied to, then made SRC_OPT, then if no mips, SHADER_READ_ONLY
                                                                       // So, this GenerateMipmaps function expects mip 0 to be in TRANSFER_SRC_OPTIMAL if called.
                                                                       // Let's assume AssetManager handles the final transition if mipLevels = 1.
                                                                       // This function should ensure all its generated mips end up SHADER_READ_ONLY.
        }


        EndSingleTimeCommands(device, commandPool, queue, commandBuffer);
        VKENG_INFO("Mipmaps generated and layouts transitioned to SHADER_READ_ONLY_OPTIMAL.");
    }


} // namespace Utils
} // namespace VulkEng
