#include "SamplerCache.h"
#include "core/Log.h"
#include "graphics/VulkanUtils.h" // For VK_CHECK (if used, or handle VkResult directly)

#include <algorithm> // For std::min
#include <functional> // For std::hash (used in Hasher)

namespace VulkEng {

    // --- SamplerInfoKey Equality Operator Implementation ---
    bool SamplerInfoKey::operator==(const SamplerInfoKey& other) const {
        // Compare all members that define a unique sampler state.
        // Floating point comparisons are direct here; for critical precision, an epsilon might be used,
        // but for LOD/anisotropy values, direct comparison is usually sufficient.
        return magFilter == other.magFilter &&
               minFilter == other.minFilter &&
               mipmapMode == other.mipmapMode &&
               addressModeU == other.addressModeU &&
               addressModeV == other.addressModeV &&
               addressModeW == other.addressModeW &&
               mipLodBias == other.mipLodBias && // Compare float
               anisotropyEnable == other.anisotropyEnable &&
               maxAnisotropy == other.maxAnisotropy && // Compare float
               compareEnable == other.compareEnable &&
               compareOp == other.compareOp &&
               minLod == other.minLod && // Compare float
               maxLod == other.maxLod && // Compare float
               borderColor == other.borderColor &&
               unnormalizedCoordinates == other.unnormalizedCoordinates;
    }

    // --- SamplerInfoKey::Hasher Implementation ---
    std::size_t SamplerInfoKey::Hasher::operator()(const SamplerInfoKey& k) const {
        std::size_t seed = 0;

        // A common way to combine hashes (similar to Boost's hash_combine)
        auto combine_hash = [&](std::size_t& current_seed, auto val) {
            current_seed ^= std::hash<decltype(val)>()(val) + 0x9e3779b9 + (current_seed << 6) + (current_seed >> 2);
        };

        combine_hash(seed, static_cast<int>(k.magFilter));
        combine_hash(seed, static_cast<int>(k.minFilter));
        combine_hash(seed, static_cast<int>(k.mipmapMode));
        combine_hash(seed, static_cast<int>(k.addressModeU));
        combine_hash(seed, static_cast<int>(k.addressModeV));
        combine_hash(seed, static_cast<int>(k.addressModeW));
        combine_hash(seed, k.mipLodBias);
        combine_hash(seed, k.anisotropyEnable);
        combine_hash(seed, k.maxAnisotropy);
        combine_hash(seed, k.compareEnable);
        combine_hash(seed, static_cast<int>(k.compareOp));
        combine_hash(seed, k.minLod);
        combine_hash(seed, k.maxLod);
        combine_hash(seed, static_cast<int>(k.borderColor));
        combine_hash(seed, k.unnormalizedCoordinates);

        return seed;
    }


    // --- SamplerCache Implementation ---

    SamplerCache::SamplerCache(VkDevice device, VkPhysicalDevice physicalDevice)
        : m_Device(device), m_PhysicalDevice(physicalDevice), m_MaxDeviceAnisotropy(1.0f), m_CachedDefaultSampler(VK_NULL_HANDLE)
    {
        if (!m_Device || !m_PhysicalDevice) {
            VKENG_CRITICAL("SamplerCache: VkDevice or VkPhysicalDevice is NULL during construction!");
            throw std::runtime_error("SamplerCache requires valid Vulkan device handles.");
        }

        // Get and store the maximum anisotropy supported by the device once.
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
        m_MaxDeviceAnisotropy = properties.limits.maxSamplerAnisotropy;

        VKENG_INFO("SamplerCache Initialized (Device Max Anisotropy: {}).", m_MaxDeviceAnisotropy);

        // Create and cache a default sampler upon initialization.
        SamplerInfoKey defaultInfo{}; // Uses default-initialized values from SamplerInfoKey struct
        // Adjust defaultInfo.maxLod if your textures commonly have mips.
        // For a true "default", usually set to the number of mips of a "standard" texture or 1 if no mips typical.
        // VK_LOD_CLAMP_NONE (a large float value) indicates using all available mip levels.
        defaultInfo.maxLod = VK_LOD_CLAMP_NONE; // Or e.g., 16.0f if you cap max mips
        defaultInfo.anisotropyEnable = true; // Enable by default
        defaultInfo.maxAnisotropy = m_MaxDeviceAnisotropy; // Use device max for default

        m_CachedDefaultSampler = GetOrCreateSampler(defaultInfo);
        if (m_CachedDefaultSampler == VK_NULL_HANDLE) {
             VKENG_ERROR("SamplerCache: Failed to create the default sampler during initialization!");
             // This might be a critical error depending on how default sampler is used.
        } else {
             VKENG_INFO("SamplerCache: Default sampler created and cached.");
        }
    }

    SamplerCache::~SamplerCache() {
        VKENG_INFO("Destroying SamplerCache and {} cached samplers...", m_SamplerMap.size());
        // Destroy all VkSampler objects stored in the cache.
        for (auto const& [key, sampler] : m_SamplerMap) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_Device, sampler, nullptr);
            }
        }
        m_SamplerMap.clear();
        m_CachedDefaultSampler = VK_NULL_HANDLE; // Should have been in the map and destroyed
        VKENG_INFO("SamplerCache Destroyed.");
    }

    VkSampler SamplerCache::GetOrCreateSampler(const SamplerInfoKey& info) {
        // 1. Check if a sampler with these exact properties already exists in the cache.
        auto it = m_SamplerMap.find(info);
        if (it != m_SamplerMap.end()) {
            // VKENG_TRACE("Sampler cache hit for sampler.");
            return it->second; // Return the existing sampler handle.
        }

        // 2. If not found, create a new sampler with the given properties.
        // VKENG_TRACE("Sampler cache miss. Creating new sampler.");
        VkSampler newSampler = CreateNewSampler(info);

        // 3. Store the newly created sampler in the cache for future reuse.
        if (newSampler != VK_NULL_HANDLE) {
            m_SamplerMap[info] = newSampler; // Using operator[] inserts if key doesn't exist.
        } else {
            VKENG_ERROR("SamplerCache: Failed to create a new sampler with the specified info.");
            // Return VK_NULL_HANDLE or perhaps the default sampler as a fallback?
            // Returning null is safer to indicate creation failure.
        }
        return newSampler;
    }

    VkSampler SamplerCache::GetDefaultSampler() {
        // The default sampler is created and cached in the constructor.
        if (m_CachedDefaultSampler == VK_NULL_HANDLE) {
            VKENG_WARN("SamplerCache: Default sampler was not created or is null. Attempting to create now.");
            // This could happen if the initial creation failed. Attempt to recreate.
             SamplerInfoKey defaultInfo{};
             defaultInfo.maxLod = VK_LOD_CLAMP_NONE;
             defaultInfo.anisotropyEnable = true;
             defaultInfo.maxAnisotropy = m_MaxDeviceAnisotropy;
             m_CachedDefaultSampler = GetOrCreateSampler(defaultInfo); // Will create if not found
             if(m_CachedDefaultSampler == VK_NULL_HANDLE) {
                 VKENG_ERROR("SamplerCache: Critical failure to create default sampler on demand.");
             }
        }
        return m_CachedDefaultSampler;
    }

    VkSampler SamplerCache::CreateNewSampler(const SamplerInfoKey& info) {
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = info.magFilter;
        samplerCreateInfo.minFilter = info.minFilter;
        samplerCreateInfo.mipmapMode = info.mipmapMode;
        samplerCreateInfo.addressModeU = info.addressModeU;
        samplerCreateInfo.addressModeV = info.addressModeV;
        samplerCreateInfo.addressModeW = info.addressModeW;
        samplerCreateInfo.mipLodBias = info.mipLodBias;

        samplerCreateInfo.anisotropyEnable = info.anisotropyEnable ? VK_TRUE : VK_FALSE;
        if (info.anisotropyEnable) {
            // Clamp requested anisotropy to the device's supported maximum.
            samplerCreateInfo.maxAnisotropy = std::min(info.maxAnisotropy, m_MaxDeviceAnisotropy);
        } else {
            samplerCreateInfo.maxAnisotropy = 1.0f; // Must be 1.0 if anisotropy is disabled
        }

        samplerCreateInfo.compareEnable = info.compareEnable ? VK_TRUE : VK_FALSE;
        samplerCreateInfo.compareOp = info.compareOp;
        samplerCreateInfo.minLod = info.minLod;
        samplerCreateInfo.maxLod = info.maxLod; // Ensure this is set correctly based on texture mip levels
        samplerCreateInfo.borderColor = info.borderColor;
        samplerCreateInfo.unnormalizedCoordinates = info.unnormalizedCoordinates ? VK_TRUE : VK_FALSE;

        VkSampler sampler = VK_NULL_HANDLE;
        VkResult result = vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &sampler);
        if (result != VK_SUCCESS) {
            VKENG_ERROR("Failed to create VkSampler! Error: {}", result);
            // Consider throwing an exception or returning VK_NULL_HANDLE more explicitly
            return VK_NULL_HANDLE;
        }

        // VKENG_TRACE("VkSampler created with MaxLOD: {}, Anisotropy: {} (Enabled: {}).",
        //             samplerCreateInfo.maxLod, samplerCreateInfo.maxAnisotropy, samplerCreateInfo.anisotropyEnable == VK_TRUE);
        return sampler;
    }

} // namespace VulkEng
