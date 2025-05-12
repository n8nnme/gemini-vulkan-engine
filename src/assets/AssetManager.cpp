#include "AssetManager.h"
#include "graphics/VulkanContext.h"    // For device, physicalDevice
#include "graphics/CommandManager.h"   // For command pool, queue
#include "graphics/Buffer.h"           // For VulkanBuffer
#include "graphics/VulkanUtils.h"      // For utility functions (copy, transition, mips)
#include "graphics/SamplerCache.h"     // For managing samplers
#include "graphics/Renderer.h"         // Needed to get Material DescriptorSetLayout & Pool (via ServiceLocator)
#include "ModelLoader.h"               // For LoadedModelData, MaterialDataSource
#include "core/Log.h"
#include "core/ServiceLocator.h"       // To get Renderer instance

#include <stdexcept>  // For std::runtime_error
#include <filesystem> // For path manipulation (C++17)
#include <algorithm>  // For std::replace, std::min, std::max
#include <cmath>      // For std::floor, std::log2

// Define STB_IMAGE_IMPLEMENTATION in ONE .cpp file (this one is suitable)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace VulkEng {

    AssetManager::AssetManager(VulkanContext& context, CommandManager& commandManager)
        : m_Context(context), m_CommandManager(commandManager)
    {
        VKENG_INFO("AssetManager: Initializing...");
        m_SamplerCache = std::make_unique<SamplerCache>(m_Context.device, m_Context.physicalDevice);
        CreateDefaultAssets(); // Create default white texture and material
        VKENG_INFO("AssetManager: Initialized.");
    }

    AssetManager::~AssetManager() {
        VKENG_INFO("AssetManager: Destroying...");

        // Textures need to be cleaned up before SamplerCache (if samplers were unique per texture)
        // or if Texture struct held unique samplers.
        // Since Texture::DestroyImageResources no longer touches sampler, order is less critical here.
        for (auto& texture : m_LoadedTextures) {
            texture.DestroyImageResources(m_Context.device);
        }
        m_LoadedTextures.clear();
        VKENG_INFO("AssetManager: Textures' GPU image resources released.");

        // SamplerCache destructor will clean up all VkSampler objects it created.
        m_SamplerCache.reset();
        VKENG_INFO("AssetManager: SamplerCache destroyed.");

        // m_LoadedModels contain Meshes which contain shared_ptrs to VulkanBuffers.
        // When m_LoadedModels is cleared, these shared_ptrs decrement.
        // If a VulkanBuffer is no longer referenced, its destructor will free Vulkan memory.
        m_LoadedModels.clear();
        m_CachedModelData.clear(); // Clear CPU-side data cache
        VKENG_INFO("AssetManager: Models and cached data cleared.");

        // Materials: descriptorSet handles are freed when m_DescriptorPool in Renderer is destroyed.
        // No GPU resources owned directly by Material struct beyond the VkDescriptorSet handle.
        m_LoadedMaterials.clear();
        VKENG_INFO("AssetManager: Materials cleared.");

        VKENG_INFO("AssetManager: Destroyed.");
    }

    void AssetManager::CreateDefaultAssets() {
        VKENG_INFO("AssetManager: Creating default assets...");

        // --- Default White Texture ---
        Texture defaultTex;
        defaultTex.width = 1;
        defaultTex.height = 1;
        defaultTex.mipLevels = 1;
        defaultTex.path = "DEFAULT_WHITE_TEXTURE";

        unsigned char whitePixel[] = {255, 255, 255, 255}; // RGBA
        VkDeviceSize imageSize = sizeof(whitePixel);

        std::unique_ptr<VulkanBuffer> stagingBuffer = CreateDeviceLocalBuffer(whitePixel, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        // Note: CreateDeviceLocalBuffer creates a DEVICE_LOCAL buffer. For staging to image, we need a HOST_VISIBLE staging.
        // Let's refine the staging process here for textures.

        VulkanBuffer pixelStagingBuffer(m_Context, imageSize, 1,
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        pixelStagingBuffer.WriteToBuffer(whitePixel, imageSize);

        VkFormat defaultTexFormat = VK_FORMAT_R8G8B8A8_UNORM; // Or SRGB if preferred for default
        Utils::createImage(m_Context.device, m_Context.physicalDevice, defaultTex.width, defaultTex.height, defaultTex.mipLevels,
                           VK_SAMPLE_COUNT_1_BIT, defaultTexFormat, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           defaultTex.image, defaultTex.imageMemory);

        Utils::TransitionImageLayout(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                     defaultTex.image, defaultTexFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, defaultTex.mipLevels);
        Utils::CopyBufferToImage(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                 pixelStagingBuffer.GetBuffer(), defaultTex.image, defaultTex.width, defaultTex.height);
        Utils::TransitionImageLayout(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                     defaultTex.image, defaultTexFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, defaultTex.mipLevels);

        defaultTex.imageView = Utils::createImageView(m_Context.device, defaultTex.image, defaultTexFormat, VK_IMAGE_ASPECT_COLOR_BIT, defaultTex.mipLevels);
        defaultTex.sampler = m_SamplerCache->GetDefaultSampler(); // Get from cache

        m_DefaultWhiteTexture = m_LoadedTextures.size();
        m_LoadedTextures.push_back(std::move(defaultTex)); // std::move shouldn't be needed for struct
        m_TexturePathToHandleMap[defaultTex.path] = m_DefaultWhiteTexture;
        VKENG_INFO("AssetManager: Default white texture created (Handle: {}).", m_DefaultWhiteTexture);

        // --- Default Material (uses default white texture) ---
        Material defaultMat("DEFAULT_ENGINE_MATERIAL");
        defaultMat.diffuseTexture = m_DefaultWhiteTexture;
        defaultMat.baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f); // A light grey

        // Allocate a descriptor set for the default material
        Renderer& renderer = ServiceLocator::GetRenderer(); // Assumes Renderer is already provided
        VkDescriptorSetLayout defaultMaterialLayout = renderer.m_MaterialDescriptorSetLayout; // Friend access or getter
        VkDescriptorPool defaultDescriptorPool = renderer.m_DescriptorPool; // Friend access or getter

        if (defaultMaterialLayout != VK_NULL_HANDLE && defaultDescriptorPool != VK_NULL_HANDLE) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = defaultDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &defaultMaterialLayout;
            VK_CHECK(vkAllocateDescriptorSets(m_Context.device, &allocInfo, &defaultMat.descriptorSet));

            const Texture& boundTexture = GetTexture(defaultMat.diffuseTexture);
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = boundTexture.imageView;
            imageInfo.sampler = boundTexture.sampler;

            VkWriteDescriptorSet descriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descriptorWrite.dstSet = defaultMat.descriptorSet;
            descriptorWrite.dstBinding = 0; // Binding 0 in Material Layout (Set 1)
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(m_Context.device, 1, &descriptorWrite, 0, nullptr);
        } else {
             VKENG_ERROR("AssetManager: Cannot create descriptor set for default material (layout or pool is null).");
        }


        m_DefaultMaterial = m_LoadedMaterials.size();
        m_LoadedMaterials.push_back(std::move(defaultMat));
        m_MaterialNameToHandleMap[defaultMat.name] = m_DefaultMaterial;
        VKENG_INFO("AssetManager: Default material created (Handle: {}).", m_DefaultMaterial);
    }

    VkSampler AssetManager::GetDefaultSampler() const {
        return m_SamplerCache->GetDefaultSampler();
    }


    TextureHandle AssetManager::LoadTexture(const std::string& filepath, bool generateMips /*= true*/) {
        std::filesystem::path canonicalPath;
        try {
            canonicalPath = std::filesystem::weakly_canonical(filepath); // More robust for relative paths
        } catch (const std::filesystem::filesystem_error& e) {
            VKENG_ERROR("AssetManager: Filesystem error for texture path '{}': {}. Using original.", filepath, e.what());
            canonicalPath = filepath;
        }
        std::string canonicalPathStr = canonicalPath.string();
        std::replace(canonicalPathStr.begin(), canonicalPathStr.end(), '\\', '/'); // Normalize

        auto it = m_TexturePathToHandleMap.find(canonicalPathStr);
        if (it != m_TexturePathToHandleMap.end()) {
            return it->second;
        }
        VKENG_INFO("AssetManager: Loading texture: {}", canonicalPathStr);

        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4; // 4 bytes for RGBA

        if (!pixels) {
            VKENG_ERROR("AssetManager: Failed to load texture image from '{}': {}", filepath, stbi_failure_reason());
            return InvalidTextureHandle;
        }

        VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB; // Common choice for color data
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(m_Context.physicalDevice, textureFormat, &formatProperties);

        bool canBlit = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
                       (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);
        bool canFilterLinear = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

        if (generateMips && !canBlit) {
            VKENG_WARN("AssetManager: Format {} for '{}' lacks BLIT SRC/DST support. Disabling mipmap generation.", textureFormat, canonicalPathStr);
            generateMips = false;
        }
        if (generateMips && !canFilterLinear) {
             VKENG_WARN("AssetManager: Format {} for '{}' lacks LINEAR filter support for blitting. Mips might use NEAREST.", textureFormat, canonicalPathStr);
        }

        uint32_t mipLevels = 1;
        if (generateMips) {
            mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        }

        Texture newTexture;
        newTexture.width = static_cast<uint32_t>(texWidth);
        newTexture.height = static_cast<uint32_t>(texHeight);
        newTexture.mipLevels = mipLevels;
        newTexture.path = canonicalPathStr;

        VulkanBuffer stagingBuffer(m_Context, imageSize, 1,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.WriteToBuffer(pixels, imageSize);
        stbi_image_free(pixels);

        Utils::createImage(m_Context.device, m_Context.physicalDevice, newTexture.width, newTexture.height, newTexture.mipLevels,
                           VK_SAMPLE_COUNT_1_BIT, textureFormat, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           newTexture.image, newTexture.imageMemory);

        // Transition for initial copy
        Utils::TransitionImageLayout(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                     newTexture.image, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     newTexture.mipLevels, 0, 1, 0); // Transition all mips to DST for generation

        // Copy mip level 0
        Utils::CopyBufferToImage(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                 stagingBuffer.GetBuffer(), newTexture.image, newTexture.width, newTexture.height);

        // Generate mipmaps (Utils::GenerateMipmaps will handle further transitions)
        if (generateMips && newTexture.mipLevels > 1) {
            Utils::GenerateMipmaps(m_Context.device, m_Context.physicalDevice, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                   newTexture.image, textureFormat, texWidth, texHeight, newTexture.mipLevels);
        } else { // If no mips or only 1 level, transition the whole image to SHADER_READ_ONLY
            Utils::TransitionImageLayout(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                                         newTexture.image, textureFormat,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // After copy
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         newTexture.mipLevels, 0, 1, 0); // Transition all mips
        }
        // Note: GenerateMipmaps should leave all mip levels in SHADER_READ_ONLY_OPTIMAL.

        newTexture.imageView = Utils::createImageView(m_Context.device, newTexture.image, textureFormat, VK_IMAGE_ASPECT_COLOR_BIT, newTexture.mipLevels);

        SamplerInfoKey samplerKey{};
        samplerKey.maxLod = static_cast<float>(newTexture.mipLevels);
        samplerKey.anisotropyEnable = VK_TRUE; // Enable anisotropy by default
        samplerKey.maxAnisotropy = m_SamplerCache->GetMaxDeviceAnisotropy(); // Use device max
        // Other samplerKey fields use defaults (linear, repeat)
        newTexture.sampler = m_SamplerCache->GetOrCreateSampler(samplerKey);

        TextureHandle newHandle = m_LoadedTextures.size();
        m_LoadedTextures.push_back(newTexture); // No std::move for plain struct
        m_TexturePathToHandleMap[canonicalPathStr] = newHandle;
        VKENG_INFO("AssetManager: Texture loaded: '{}' (Handle: {}, Mips: {}).", canonicalPathStr, newHandle, newTexture.mipLevels);
        return newHandle;
    }

    const Texture& AssetManager::GetTexture(TextureHandle handle) const {
        if (handle == InvalidTextureHandle || handle >= m_LoadedTextures.size()) {
            VKENG_WARN("AssetManager: Invalid texture handle {} requested. Returning default white texture.", handle);
            return m_LoadedTextures[m_DefaultWhiteTexture]; // Assumes default is always valid at index 0 or similar
        }
        return m_LoadedTextures[handle];
    }

    const Material& AssetManager::GetMaterial(MaterialHandle handle) const {
        if (handle == InvalidMaterialHandle || handle >= m_LoadedMaterials.size()) {
            VKENG_WARN("AssetManager: Invalid material handle {} requested. Returning default material.", handle);
            return m_LoadedMaterials[m_DefaultMaterial];
        }
        return m_LoadedMaterials[handle];
    }

    ModelHandle AssetManager::LoadModel(const std::string& filepath) {
        std::filesystem::path canonicalPath;
        try { canonicalPath = std::filesystem::weakly_canonical(filepath); }
        catch (const std::filesystem::filesystem_error& e) {
            VKENG_ERROR("AssetManager: Filesystem error for model path '{}': {}", filepath, e.what());
            canonicalPath = filepath;
        }
        std::string canonicalPathStr = canonicalPath.string();
        std::replace(canonicalPathStr.begin(), canonicalPathStr.end(), '\\', '/');

        auto it = m_ModelPathToHandleMap.find(canonicalPathStr);
        if (it != m_ModelPathToHandleMap.end()) {
            return it->second;
        }
        VKENG_INFO("AssetManager: Loading Model: {}", canonicalPathStr);

        LoadedModelData loadedCpuData; // Data from ModelLoader
        if (!ModelLoader::LoadModel(filepath, loadedCpuData)) {
            VKENG_ERROR("AssetManager: ModelLoader failed for: {}", filepath);
            return InvalidModelHandle;
        }

        std::vector<MaterialHandle> modelMaterialHandles;
        modelMaterialHandles.reserve(loadedCpuData.materialsFromFile.size());

        Renderer& renderer = ServiceLocator::GetRenderer();
        VkDescriptorSetLayout materialSetLayout = renderer.m_MaterialDescriptorSetLayout;
        VkDescriptorPool descriptorPool = renderer.m_DescriptorPool;

        for (const auto& matSource : loadedCpuData.materialsFromFile) {
            modelMaterialHandles.push_back(ProcessLoadedMaterial(matSource, materialSetLayout, descriptorPool));
        }

        std::vector<Mesh> gpuMeshes;
        gpuMeshes.reserve(loadedCpuData.meshesForRender.size());
        for (const auto& meshData : loadedCpuData.meshesForRender) {
            if (meshData.vertices.empty() || meshData.indices.empty()) continue;
            gpuMeshes.push_back(CreateGPUMeshFromData(meshData, modelMaterialHandles));
        }

        ModelHandle newHandle = m_LoadedModels.size();
        m_LoadedModels.push_back(std::move(gpuMeshes));
        m_ModelPathToHandleMap[canonicalPathStr] = newHandle;
        m_CachedModelData.push_back(std::move(loadedCpuData)); // Cache CPU data

        VKENG_INFO("AssetManager: Successfully loaded model '{}' (Handle: {}).", canonicalPathStr, newHandle);
        return newHandle;
    }

    const std::vector<Mesh>& AssetManager::GetModelMeshes(ModelHandle handle) const {
        if (handle == InvalidModelHandle || handle >= m_LoadedModels.size()) {
            static const std::vector<Mesh> emptyResult;
            VKENG_ERROR("AssetManager: Invalid model handle {} requested for GetModelMeshes.", handle);
            return emptyResult;
        }
        return m_LoadedModels[handle];
    }

    const LoadedModelData* AssetManager::GetLoadedModelData(ModelHandle handle) const {
        if (handle == InvalidModelHandle || handle >= m_CachedModelData.size()) {
            VKENG_ERROR("AssetManager: Invalid model handle {} requested for GetLoadedModelData.", handle);
            return nullptr;
        }
        return &m_CachedModelData[handle];
    }


    MaterialHandle AssetManager::ProcessLoadedMaterial(
        const MaterialDataSource& matDataSource,
        VkDescriptorSetLayout materialSetLayout,
        VkDescriptorPool descriptorPool)
    {
        // Check cache by name (simple approach)
        auto matIt = m_MaterialNameToHandleMap.find(matDataSource.name);
        if (matIt != m_MaterialNameToHandleMap.end()) {
            return matIt->second;
        }

        Material newMaterial(matDataSource.name); // Use constructor with name
        newMaterial.baseColorFactor = matDataSource.baseColorFactor;

        if (!matDataSource.diffuseTexturePath.empty()) {
            newMaterial.diffuseTexture = LoadTexture(matDataSource.diffuseTexturePath, true); // Generate mips
            if (newMaterial.diffuseTexture == InvalidTextureHandle) {
                newMaterial.diffuseTexture = GetDefaultWhiteTexture();
            }
        } else {
            newMaterial.diffuseTexture = GetDefaultWhiteTexture();
        }
        // TODO: Process other textures (normal, metallicRoughness etc.) from matDataSource

        // Allocate and Update Descriptor Set for this Material's texture
        if (materialSetLayout != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE) {
            VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &materialSetLayout;

            VkResult result = vkAllocateDescriptorSets(m_Context.device, &allocInfo, &newMaterial.descriptorSet);
            if (result == VK_SUCCESS && newMaterial.descriptorSet != VK_NULL_HANDLE) {
                const Texture& textureToBind = GetTexture(newMaterial.diffuseTexture);
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = textureToBind.imageView;
                imageInfo.sampler = textureToBind.sampler;

                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = newMaterial.descriptorSet;
                write.dstBinding = 0; // Binding 0 in Material Layout (Set 1) for diffuse texture
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.descriptorCount = 1;
                write.pImageInfo = &imageInfo;
                vkUpdateDescriptorSets(m_Context.device, 1, &write, 0, nullptr);
            } else {
                VKENG_ERROR("AssetManager: Failed to allocate/update descriptor set for material '{}'. Result: {}", newMaterial.name, result);
                newMaterial.descriptorSet = VK_NULL_HANDLE; // Mark as invalid
                // Potentially assign default material's descriptor set if available
                newMaterial.diffuseTexture = GetDefaultWhiteTexture(); // Fallback texture
                const Material& defaultMat = GetMaterial(GetDefaultMaterial());
                newMaterial.descriptorSet = defaultMat.descriptorSet;
            }
        } else {
             VKENG_ERROR("AssetManager: Material layout or descriptor pool is NULL for material '{}'.", newMaterial.name);
             newMaterial.descriptorSet = GetMaterial(GetDefaultMaterial()).descriptorSet; // Use default material's set
        }

        MaterialHandle newHandle = m_LoadedMaterials.size();
        m_LoadedMaterials.push_back(std::move(newMaterial));
        m_MaterialNameToHandleMap[newMaterial.name] = newHandle;
        return newHandle;
    }


    Mesh AssetManager::CreateGPUMeshFromData(const MeshData& meshData, const std::vector<MaterialHandle>& materialHandlesForModel) {
        Mesh gpuMesh;
        gpuMesh.name = meshData.name;

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * meshData.vertices.size();
        gpuMesh.vertexBuffer = CreateDeviceLocalBuffer(meshData.vertices.data(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
        gpuMesh.vertexBufferOffset = 0;

        VkDeviceSize indexBufferSize = sizeof(uint32_t) * meshData.indices.size();
        gpuMesh.indexBuffer = CreateDeviceLocalBuffer(meshData.indices.data(), indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
        gpuMesh.indexBufferOffset = 0;

        if (meshData.materialIndex < materialHandlesForModel.size()) {
            gpuMesh.material = materialHandlesForModel[meshData.materialIndex];
        } else {
            VKENG_WARN("AssetManager: Invalid material index ({}) for mesh '{}'. Using default material.", meshData.materialIndex, meshData.name);
            gpuMesh.material = GetDefaultMaterial();
        }
        return gpuMesh;
    }

    std::unique_ptr<VulkanBuffer> AssetManager::CreateDeviceLocalBuffer(const void* data, VkDeviceSize bufferSize, VkBufferUsageFlags usage) {
        if (bufferSize == 0 || data == nullptr) {
            VKENG_ERROR("AssetManager::CreateDeviceLocalBuffer: Invalid data or zero buffer size.");
            return nullptr; // Or throw
        }
        VulkanBuffer stagingBuffer(m_Context, bufferSize, 1,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.WriteToBuffer(data, bufferSize);

        auto deviceBuffer = std::make_unique<VulkanBuffer>(
            m_Context, bufferSize, 1,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Utils::CopyBuffer(m_Context.device, m_CommandManager.GetCommandPool(), m_Context.graphicsQueue,
                          stagingBuffer.GetBuffer(), deviceBuffer->GetBuffer(), bufferSize);
        return deviceBuffer;
    }

} // namespace VulkEng
