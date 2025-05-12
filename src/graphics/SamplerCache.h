#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map> // For the cache
#include <functional>    // For std::hash (used by Hasher)
#include <cstdint>       // For uint32_t

namespace VulkEng {

    // Structure to define all relevant sampler properties.
    // This will be used as the key in our sampler cache.
    struct SamplerInfoKey {
        VkFilter magFilter = VK_FILTER_LINEAR;
        VkFilter minFilter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        float mipLodBias = 0.0f;
        bool anisotropyEnable = true; // Default to enabled if supported
        float maxAnisotropy = 16.0f;  // Request a high value, will be clamped by device limits
        bool compareEnable = false;   // For shadow mapping / percentage-closer filtering
        VkCompareOp compareOp = VK_COMPARE_OP_NEVER; // Default, only relevant if compareEnable is true
        float minLod = 0.0f;
        float maxLod = VK_LOD_CLAMP_NONE; // Use a large value to indicate all mip levels, or specific number.
                                          // VK_LOD_CLAMP_NONE often equates to a large float like 1000.0f or max float.
                                          // Or specific number of mip levels for the texture.
        VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // For clamp to border
        bool unnormalizedCoordinates = false;

        // Equality operator for std::unordered_map key comparison.
        bool operator==(const SamplerInfoKey& other) const;

        // Custom Hasher struct for std::unordered_map.
        // Needs to be defined for SamplerInfoKey to be used as a key.
        struct Hasher;
    };

    // Definition of the custom Hasher for SamplerInfoKey.
    // This must be defined before SamplerCache uses it in its unordered_map.
    struct SamplerInfoKey::Hasher {
        std::size_t operator()(const SamplerInfoKey& k) const;
    };


    // Manages the creation, caching, and reuse of VkSampler objects
    // to avoid redundant sampler creation.
    class SamplerCache {
    public:
        // Constructor takes the logical and physical device handles.
        SamplerCache(VkDevice device, VkPhysicalDevice physicalDevice);
        // Destructor cleans up all cached samplers.
        ~SamplerCache();

        // Prevent copying and assignment.
        SamplerCache(const SamplerCache&) = delete;
        SamplerCache& operator=(const SamplerCache&) = delete;

        // Retrieves an existing sampler matching the info, or creates and caches a new one.
        // Returns VK_NULL_HANDLE on failure to create.
        VkSampler GetOrCreateSampler(const SamplerInfoKey& info);

        // Gets a commonly used default sampler (e.g., linear filtering, repeat address mode, full mip range).
        // The properties of this default sampler are defined within the cache.
        VkSampler GetDefaultSampler();

        // Publicly expose device's max anisotropy if other systems need to query it
        // before creating a SamplerInfoKey.
        float GetMaxDeviceAnisotropy() const { return m_MaxDeviceAnisotropy; }

    private:
        // Internal helper to create a new VkSampler based on the provided info.
        VkSampler CreateNewSampler(const SamplerInfoKey& info);

        VkDevice m_Device = VK_NULL_HANDLE;             // Logical device handle (non-owning)
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // Physical device handle (non-owning)
        float m_MaxDeviceAnisotropy = 1.0f;         // Max anisotropy supported by the physical device

        // The cache: maps SamplerInfoKey (properties) to the VkSampler handle.
        std::unordered_map<SamplerInfoKey, VkSampler, SamplerInfoKey::Hasher> m_SamplerMap;

        VkSampler m_CachedDefaultSampler = VK_NULL_HANDLE; // Cache the most common default sampler
    };

} // namespace VulkEng
