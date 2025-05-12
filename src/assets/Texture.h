#pragma once

#include <vulkan/vulkan.h> // For Vulkan types (VkImage, VkSampler, etc.)
#include <string>          // For std::string (texture path)
#include <memory>          // Not strictly needed here anymore unless sharing samplers via smart_ptr
#include "core/Log.h"      // For logging cleanup actions (optional)

namespace VulkEng {

    // Represents a texture asset, including its Vulkan image, view, memory,
    // and a handle to a sampler (which is likely managed by a SamplerCache).
    struct Texture {
        VkImage image = VK_NULL_HANDLE;           // Handle to the Vulkan image object
        VkDeviceMemory imageMemory = VK_NULL_HANDLE; // Device memory allocated for the image
        VkImageView imageView = VK_NULL_HANDLE;     // Image view for accessing the image
        VkSampler sampler = VK_NULL_HANDLE;         // Sampler used for this texture (obtained from SamplerCache)

        uint32_t width = 0;    // Width of the texture in pixels
        uint32_t height = 0;   // Height of the texture in pixels
        uint32_t mipLevels = 1;// Number of mipmap levels
        std::string path;      // Original file path of the texture, for debugging or identification

        // Destructor-like method to clean up Vulkan resources associated with this texture.
        // IMPORTANT: This method should ONLY release resources directly owned by this struct instance
        // (image, imageView, imageMemory). The VkSampler is typically owned by a SamplerCache
        // and should not be destroyed here.
        // Call this explicitly before the Vulkan device is destroyed if not using RAII wrappers for Vulkan objects.
        void DestroyImageResources(VkDevice device) {
            // VKENG_TRACE("Destroying image resources for texture: {}", path.empty() ? "Unnamed/Default" : path);

            if (imageView != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
            if (image != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
            }
            if (imageMemory != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
                vkFreeMemory(device, imageMemory, nullptr);
                imageMemory = VK_NULL_HANDLE;
            }

            // Sampler is managed by SamplerCache, so just nullify the handle here.
            // The actual VkSampler object will be destroyed by the SamplerCache when it's no longer needed.
            sampler = VK_NULL_HANDLE;
        }
    };

    // Handle type for referencing textures managed by the AssetManager.
    // Using size_t as a simple index into a vector.
    using TextureHandle = size_t;
    // A special handle value to indicate an invalid or unassigned texture.
    const TextureHandle InvalidTextureHandle = static_cast<TextureHandle>(-1); // Or std::numeric_limits<size_t>::max();

} // namespace VulkEng


// 12 May 20:00 | I am a vegetable
// #pragma once
// 
// #include <vulkan/vulkan.h>
// #include <string>
// #include <memory>
// #include "core/Log.h" // For logging cleanup
// 
// namespace VulkEng {
// 
//     struct Texture {
//         VkImage image = VK_NULL_HANDLE;
//         VkDeviceMemory imageMemory = VK_NULL_HANDLE;
//         VkImageView imageView = VK_NULL_HANDLE;
//         VkSampler sampler = VK_NULL_HANDLE; // Handle obtained from SamplerCache
// 
//         uint32_t width = 0;
//         uint32_t height = 0;
//         uint32_t mipLevels = 1;
//         std::string path; // Original path for debugging/lookup
// 
//         // Cleanup for image and view, NOT sampler
//         void DestroyImageResources(VkDevice device) {
//              if (imageView != VK_NULL_HANDLE) {
//                 vkDestroyImageView(device, imageView, nullptr);
//                 imageView = VK_NULL_HANDLE;
//             }
//              if (image != VK_NULL_HANDLE) {
//                 vkDestroyImage(device, image, nullptr);
//                 image = VK_NULL_HANDLE;
//             }
//              if (imageMemory != VK_NULL_HANDLE) {
//                 vkFreeMemory(device, imageMemory, nullptr);
//                 imageMemory = VK_NULL_HANDLE;
//             }
//             sampler = VK_NULL_HANDLE; // Clear the handle, cache owns the sampler object
//         }
//     };
// 
//     using TextureHandle = size_t;
//     const TextureHandle InvalidTextureHandle = static_cast<TextureHandle>(-1);
// 
// } // namespace VulkEng
