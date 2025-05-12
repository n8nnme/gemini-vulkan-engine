#include "Renderer.h"
#include "VulkanUtils.h"
#include "core/Window.h"
#include "core/Log.h"
#include "core/ServiceLocator.h" // For AssetManager, UIManager
#include "assets/Mesh.h"
#include "assets/Material.h"
#include "assets/Texture.h"
#include "assets/AssetManager.h"
#include "scene/Components/CameraComponent.h"
#include "scene/Components/TransformComponent.h"
#include "ui/UIManager.h"


#include <GLFW/glfw3.h> // For glfwGetTime in UBO update (can be removed)
#include <fstream>
#include <stdexcept>
#include <array>
#include <vector>
#include <chrono> // For UBO update example
#include <glm/gtc/type_ptr.hpp> // For glm::value_ptr

namespace VulkEng {

    // --- Renderer Constructor / Destructor ---
    Renderer::Renderer(Window& window)
        : m_Window(window),
          m_VulkanContext(nullptr), m_Swapchain(nullptr), m_CommandManager(nullptr),
          m_RenderPass(VK_NULL_HANDLE),
          m_FrameDescriptorSetLayout(VK_NULL_HANDLE), m_MaterialDescriptorSetLayout(VK_NULL_HANDLE),
          m_PipelineLayout(VK_NULL_HANDLE), m_GraphicsPipeline(VK_NULL_HANDLE),
          m_DepthImage(VK_NULL_HANDLE), m_DepthImageMemory(VK_NULL_HANDLE), m_DepthImageView(VK_NULL_HANDLE),
          m_DescriptorPool(VK_NULL_HANDLE),
          m_CurrentFrameIndex(0), m_CurrentImageIndex(0), m_FramebufferResized(false)
    {
        InitVulkan();
    }

    Renderer::~Renderer() {
        VKENG_INFO("Destroying Renderer...");
        if (m_VulkanContext && m_VulkanContext->device != VK_NULL_HANDLE) {
            WaitForDeviceIdle();
        }

        CleanupSwapchainDependents(); // Pipelines, framebuffers, depth, render pass

        // Destroy UBO buffers
        m_UniformBuffers.clear();
        m_LightUniformBuffers.clear();
        VKENG_INFO("UBO Buffers destroyed.");

        // Descriptor Set Layouts
        if (m_VulkanContext && m_VulkanContext->device != VK_NULL_HANDLE) {
            if (m_FrameDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_VulkanContext->device, m_FrameDescriptorSetLayout, nullptr);
                m_FrameDescriptorSetLayout = VK_NULL_HANDLE;
            }
            if (m_MaterialDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_VulkanContext->device, m_MaterialDescriptorSetLayout, nullptr);
                m_MaterialDescriptorSetLayout = VK_NULL_HANDLE;
            }
            // Pipeline Layout is destroyed before pipeline usually (or with it)
            if (m_PipelineLayout != VK_NULL_HANDLE) { // Should be destroyed by CleanupSwapchainDependents
                 // vkDestroyPipelineLayout(m_VulkanContext->device, m_PipelineLayout, nullptr);
                 // m_PipelineLayout = VK_NULL_HANDLE;
            }
             VKENG_INFO("Descriptor Set Layouts destroyed.");

            // Descriptor Pool (frees all sets allocated from it, including frame sets and material sets)
            if (m_DescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_VulkanContext->device, m_DescriptorPool, nullptr);
                m_DescriptorPool = VK_NULL_HANDLE;
                VKENG_INFO("Descriptor Pool destroyed.");
            }
            // m_FrameDescriptorSets are implicitly freed by pool destruction.

            // Sync objects
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (m_RenderFinishedSemaphores.size() > i && m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE)
                    vkDestroySemaphore(m_VulkanContext->device, m_RenderFinishedSemaphores[i], nullptr);
                if (m_ImageAvailableSemaphores.size() > i && m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE)
                    vkDestroySemaphore(m_VulkanContext->device, m_ImageAvailableSemaphores[i], nullptr);
                if (m_InFlightFences.size() > i && m_InFlightFences[i] != VK_NULL_HANDLE)
                    vkDestroyFence(m_VulkanContext->device, m_InFlightFences[i], nullptr);
            }
             VKENG_INFO("Synchronization objects destroyed.");
        }
        m_RenderFinishedSemaphores.clear();
        m_ImageAvailableSemaphores.clear();
        m_InFlightFences.clear();


        // CommandManager unique_ptr handles its own cleanup
        m_CommandManager.reset();
        // Swapchain unique_ptr handles its own cleanup
        m_Swapchain.reset();
        // VulkanContext unique_ptr handles its own cleanup
        m_VulkanContext.reset();

        VKENG_INFO("Renderer Destroyed.");
    }

    void Renderer::InitVulkan() {
        VKENG_INFO("Renderer: Initializing Vulkan Resources...");
        m_VulkanContext = std::make_unique<VulkanContext>(m_Window);
        // Pass MAX_FRAMES_IN_FLIGHT to command manager for buffer count
        m_CommandManager = std::make_unique<CommandManager>(*m_VulkanContext, MAX_FRAMES_IN_FLIGHT);
        m_Swapchain = std::make_unique<Swapchain>(*m_VulkanContext, m_Window.GetWidth(), m_Window.GetHeight());

        CreateDescriptorSetLayouts(); // For Frame UBOs (Set 0) and Material Textures (Set 1)
        CreateUniformBuffers();       // Camera UBOs
        CreateLightUniformBuffers();  // Light UBOs
        CreateDescriptorPool();       // Pool for both frame and material sets
        CreateFrameDescriptorSets();  // Sets for Set 0 (Camera + Light UBOs per frame)
                                      // Material descriptor sets (Set 1) are created by AssetManager
        CreateSyncObjects();          // Semaphores & Fences

        CreateSwapchainDependents();  // RenderPass, Pipeline, Framebuffers, Depth Buffer
                                      // Pipeline creation uses the descriptor set layouts

        // Update context with info needed by UIManager
        m_VulkanContext->mainRenderPass = m_RenderPass;
        m_VulkanContext->imageCount = m_Swapchain->GetImageCount();
        m_VulkanContext->minImageCount = m_Context.minImageCount; // Set during swapchain creation based on capabilities

        VKENG_INFO("Renderer: Vulkan Initialization Complete.");
    }

    void Renderer::CreateSyncObjects() {
        VKENG_INFO("Creating Synchronization Objects ({} frames)...", MAX_FRAMES_IN_FLIGHT);
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled for first frame

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VK_CHECK(vkCreateSemaphore(m_VulkanContext->device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(m_VulkanContext->device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]));
            VK_CHECK(vkCreateFence(m_VulkanContext->device, &fenceInfo, nullptr, &m_InFlightFences[i]));
        }
        VKENG_INFO("Synchronization Objects Created.");
    }

    void Renderer::CreateSwapchainDependents() {
        VKENG_INFO("Creating Swapchain Dependent Resources...");
        CreateDepthResources();
        CreateRenderPass();
        CreateGraphicsPipeline(); // Uses layouts, render pass
        CreateFramebuffers();
        VKENG_INFO("Swapchain Dependent Resources Created.");
    }

    void Renderer::CleanupSwapchainDependents() {
        VKENG_INFO("Cleaning up Swapchain Dependent Resources...");
        if (m_VulkanContext && m_VulkanContext->device != VK_NULL_HANDLE) {
            if (m_DepthImageView != VK_NULL_HANDLE) vkDestroyImageView(m_VulkanContext->device, m_DepthImageView, nullptr);
            if (m_DepthImage != VK_NULL_HANDLE) vkDestroyImage(m_VulkanContext->device, m_DepthImage, nullptr);
            if (m_DepthImageMemory != VK_NULL_HANDLE) vkFreeMemory(m_VulkanContext->device, m_DepthImageMemory, nullptr);
            m_DepthImageView = VK_NULL_HANDLE; m_DepthImage = VK_NULL_HANDLE; m_DepthImageMemory = VK_NULL_HANDLE;

            for (auto framebuffer : m_SwapChainFramebuffers) {
                if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(m_VulkanContext->device, framebuffer, nullptr);
            }
            m_SwapChainFramebuffers.clear();

            if (m_GraphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_VulkanContext->device, m_GraphicsPipeline, nullptr);
            if (m_PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_VulkanContext->device, m_PipelineLayout, nullptr);
            m_GraphicsPipeline = VK_NULL_HANDLE; m_PipelineLayout = VK_NULL_HANDLE;

            if (m_RenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(m_VulkanContext->device, m_RenderPass, nullptr);
            m_RenderPass = VK_NULL_HANDLE;
        }
        VKENG_INFO("Swapchain Dependent Resources Cleaned Up.");
    }

    void Renderer::RecreateSwapchain() {
        VKENG_WARN("Recreating Swapchain...");
        int width = 0, height = 0;
        m_Window.GetFramebufferSize(width, height);
        while (width == 0 || height == 0) {
            m_Window.GetFramebufferSize(width, height);
            glfwWaitEvents();
        }
        WaitForDeviceIdle();
        CleanupSwapchainDependents();
        m_Swapchain->Recreate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        // Command buffers in CommandManager are tied to MAX_FRAMES_IN_FLIGHT, not swapchain image count directly,
        // so they usually don't need recreation unless MAX_FRAMES_IN_FLIGHT changes.
        CreateSwapchainDependents();
        // Update context with new swapchain details
        m_VulkanContext->mainRenderPass = m_RenderPass;
        m_VulkanContext->imageCount = m_Swapchain->GetImageCount();
        // m_VulkanContext->minImageCount was set by swapchain creation
        m_FramebufferResized = false;
        VKENG_INFO("Swapchain Recreated.");
    }

    void Renderer::HandleResize(int width, int height) {
        m_FramebufferResized = true;
    }

    bool Renderer::BeginFrame() {
        VK_CHECK(vkWaitForFences(m_VulkanContext->device, 1, &m_InFlightFences[m_CurrentFrameIndex], VK_TRUE, UINT64_MAX));

        VkResult result = vkAcquireNextImageKHR(
            m_VulkanContext->device, m_Swapchain->GetSwapchain(), UINT64_MAX,
            m_ImageAvailableSemaphores[m_CurrentFrameIndex], VK_NULL_HANDLE, &m_CurrentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain(); return false;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            VKENG_ERROR("Failed to acquire swap chain image! Result: {}", result);
            throw std::runtime_error("Failed to acquire swap chain image!");
        } else if (result == VK_SUBOPTIMAL_KHR) {
             VKENG_WARN("Swapchain suboptimal. Flagging for recreation.");
             m_FramebufferResized = true; // Recreate at next opportunity
        }

        VK_CHECK(vkResetFences(m_VulkanContext->device, 1, &m_InFlightFences[m_CurrentFrameIndex]));
        if (!m_CommandManager->BeginFrame(m_CurrentFrameIndex)) { // BeginFrame in CommandManager resets and begins
            VKENG_ERROR("Failed to begin command buffer for frame {}!", m_CurrentFrameIndex);
            return false;
        }
        return true;
    }

    void Renderer::RecordCommands(const std::vector<RenderObjectInfo>& renderables, CameraComponent* camera) {
        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        AssetManager& assetManager = ServiceLocator::GetAssetManager();
        UIManager& uiManager = ServiceLocator::GetUIManager();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_RenderPass;
        renderPassInfo.framebuffer = m_SwapChainFramebuffers[m_CurrentImageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_Swapchain->GetExtent();
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

        VkViewport viewport{}; /* ... set viewport based on swapchain extent ... */
        viewport.x = 0.0f; viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Swapchain->GetExtent().width);
        viewport.height = static_cast<float>(m_Swapchain->GetExtent().height);
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor{}; /* ... set scissor based on swapchain extent ... */
        scissor.offset = {0,0}; scissor.extent = m_Swapchain->GetExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Update Frame UBOs
        if (camera) UpdateCameraUBO(m_CurrentFrameIndex, camera->GetViewMatrix(), camera->GetProjectionMatrix());
        else UpdateCameraUBO(m_CurrentFrameIndex, glm::mat4(1.0f), glm::mat4(1.0f)); // Default if no camera
        UpdateLightUBO(m_CurrentFrameIndex);

        // Bind Frame Descriptor Set (Set 0: Camera + Light)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
                                0, 1, &m_FrameDescriptorSets[m_CurrentFrameIndex], 0, nullptr);

        // Draw Scene Objects
        for (const auto& renderInfo : renderables) {
            if (!renderInfo.mesh || !renderInfo.transform || !renderInfo.mesh->vertexBuffer || !renderInfo.mesh->indexBuffer) continue;

            glm::mat4 modelMatrix = renderInfo.transform->GetWorldMatrix();
            vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), glm::value_ptr(modelMatrix));

            const Material& material = assetManager.GetMaterial(renderInfo.mesh->material);
            if (material.descriptorSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
                                        1, 1, &material.descriptorSet, 0, nullptr);
            } else {
                 VKENG_WARN_ONCE("Material '{}' (Handle {}) has NULL descriptor set. Object might render incorrectly.", material.name, renderInfo.mesh->material);
                 // Optionally bind a default material descriptor set here
            }

            VkBuffer vertexBuffers[] = {renderInfo.mesh->vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {renderInfo.mesh->vertexBufferOffset};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, renderInfo.mesh->indexBuffer->GetBuffer(), renderInfo.mesh->indexBufferOffset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, renderInfo.mesh->indexCount, 1, 0, 0, 0);
        }

        // Render ImGui
        uiManager.RenderDrawData(commandBuffer);

        vkCmdEndRenderPass(commandBuffer);
    }

    void Renderer::EndFrameAndPresent() {
        m_CommandManager->EndFrameRecording(m_CurrentFrameIndex); // Finalize command buffer recording
        VkCommandBuffer commandBuffer = m_CommandManager->GetCommandBuffers()[m_CurrentFrameIndex];

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrameIndex]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrameIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(m_VulkanContext->graphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrameIndex]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {m_Swapchain->GetSwapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        VkResult result = vkQueuePresentKHR(m_VulkanContext->presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
            m_FramebufferResized = true; // Ensure flag is set for next BeginFrame to handle
            // RecreateSwapchain(); // Don't recreate here directly, let BeginFrame handle it to avoid race conditions.
        } else if (result != VK_SUCCESS) {
            VKENG_ERROR("Failed to present swap chain image! Result: {}", result);
            throw std::runtime_error("Failed to present swap chain image!");
        }

        m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Renderer::WaitForDeviceIdle() {
        if (m_VulkanContext && m_VulkanContext->device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_VulkanContext->device);
        }
    }

    VkCommandBuffer Renderer::GetCurrentCommandBuffer() {
        if (m_CurrentFrameIndex >= m_CommandManager->GetCommandBuffers().size()) {
             throw std::runtime_error("Attempted to get command buffer with invalid frame index!");
        }
        return m_CommandManager->GetCommandBuffers()[m_CurrentFrameIndex];
    }


    // --- Resource Creation Implementations ---
    void Renderer::CreateDepthResources() {
        VKENG_INFO("Creating Depth Resources...");
        m_DepthFormat = Utils::findSupportedFormat(
            m_VulkanContext->physicalDevice,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        Utils::createImage(m_VulkanContext->device, m_VulkanContext->physicalDevice,
                           m_Swapchain->GetExtent().width, m_Swapchain->GetExtent().height, 1, VK_SAMPLE_COUNT_1_BIT,
                           m_DepthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory);
        if(m_DepthImage == VK_NULL_HANDLE) throw std::runtime_error("Failed to create depth image.");

        m_DepthImageView = Utils::createImageView(m_VulkanContext->device, m_DepthImage, m_DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
        if(m_DepthImageView == VK_NULL_HANDLE) throw std::runtime_error("Failed to create depth image view.");
        // Transition layout is implicitly handled by render pass initial/final layouts and dependencies
        VKENG_INFO("Depth Resources Created (Format: {}).", m_DepthFormat);
    }

    void Renderer::CreateRenderPass() {
        VKENG_INFO("Creating Render Pass...");
        VkAttachmentDescription colorAttachment{}; /* ... setup ... */
        colorAttachment.format = m_Swapchain->GetImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // For presentation after UI

        VkAttachmentReference colorAttachmentRef{}; /* ... setup ... */
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{}; /* ... setup ... */
        depthAttachment.format = m_DepthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't need depth after pass for now
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{}; /* ... setup ... */
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{}; /* ... setup ... */
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{}; /* ... setup ... */
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;


        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{}; /* ... setup ... */
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(m_VulkanContext->device, &renderPassInfo, nullptr, &m_RenderPass));
        VKENG_INFO("Render Pass Created.");
    }

    void Renderer::CreateDescriptorSetLayouts() {
        VKENG_INFO("Creating Descriptor Set Layouts...");
        // Layout 0: Frame Data (Camera UBO + Light UBO)
        std::array<VkDescriptorSetLayoutBinding, 2> frameBindings = {};
        frameBindings[0].binding = 0; // Camera UBO
        frameBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        frameBindings[0].descriptorCount = 1;
        frameBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        frameBindings[1].binding = 1; // Light UBO
        frameBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        frameBindings[1].descriptorCount = 1;
        frameBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo frameLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        frameLayoutInfo.bindingCount = static_cast<uint32_t>(frameBindings.size());
        frameLayoutInfo.pBindings = frameBindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_VulkanContext->device, &frameLayoutInfo, nullptr, &m_FrameDescriptorSetLayout));

        // Layout 1: Material Texture Sampler
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0; // Binding 0 within Set 1
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo materialLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        materialLayoutInfo.bindingCount = 1;
        materialLayoutInfo.pBindings = &samplerLayoutBinding;
        VK_CHECK(vkCreateDescriptorSetLayout(m_VulkanContext->device, &materialLayoutInfo, nullptr, &m_MaterialDescriptorSetLayout));
        VKENG_INFO("Descriptor Set Layouts Created (Set0: Frame, Set1: Material).");
    }

    void Renderer::CreateGraphicsPipeline() {
        VKENG_INFO("Creating Graphics Pipeline...");
        auto vertShaderCode = ReadFile(SHADER_PATH_DEFINITION "simple.vert.spv");
        auto fragShaderCode = ReadFile(SHADER_PATH_DEFINITION "simple.frag.spv");
        VkShaderModule vertModule = CreateShaderModule(vertShaderCode);
        VkShaderModule fragModule = CreateShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; /* ... setup ... */
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; vertStageInfo.module = vertModule; vertStageInfo.pName = "main";
        VkPipelineShaderStageCreateInfo fragStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; /* ... setup ... */
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fragStageInfo.module = fragModule; fragStageInfo.pName = "main";
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

        auto bindingDesc = Vertex::getBindingDescription();
        auto attributeDesc = Vertex::getAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO}; /* ... setup ... */
        vertexInputInfo.vertexBindingDescriptionCount = 1; vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDesc.size()); vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; /* ... setup ... */
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; /* ... setup ... */
        viewportState.viewportCount = 1; viewportState.scissorCount = 1; // Dynamic states

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; /* ... setup ... */
        rasterizer.depthClampEnable = VK_FALSE; rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL; rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Match GLM default
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; /* ... setup ... */
        multisampling.sampleShadingEnable = VK_FALSE; multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; /* ... setup ... */
        depthStencil.depthTestEnable = VK_TRUE; depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{}; /* ... setup ... */
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // No blending for opaque
        // For alpha blending:
        // colorBlendAttachment.blendEnable = VK_TRUE;
        // colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        // colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        // colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        // colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        // colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        // colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;


        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; /* ... setup ... */
        colorBlending.logicOpEnable = VK_FALSE; colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; /* ... setup ... */
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()); dynamicStateInfo.pDynamicStates = dynamicStates.data();

        std::array<VkDescriptorSetLayout, 2> setLayouts = {m_FrameDescriptorSetLayout, m_MaterialDescriptorSetLayout};
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; pushConstantRange.offset = 0; pushConstantRange.size = sizeof(glm::mat4);
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; /* ... setup ... */
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        pipelineLayoutInfo.pSetLayouts = setLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1; pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        VK_CHECK(vkCreatePipelineLayout(m_VulkanContext->device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; /* ... setup ... */
        pipelineInfo.stageCount = 2; pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo; pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState; pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling; pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending; pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = m_PipelineLayout; pipelineInfo.renderPass = m_RenderPass; pipelineInfo.subpass = 0;
        VK_CHECK(vkCreateGraphicsPipelines(m_VulkanContext->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline));

        vkDestroyShaderModule(m_VulkanContext->device, fragModule, nullptr);
        vkDestroyShaderModule(m_VulkanContext->device, vertModule, nullptr);
        VKENG_INFO("Graphics Pipeline Created.");
    }

    void Renderer::CreateFramebuffers() {
        VKENG_INFO("Creating Framebuffers...");
        m_SwapChainFramebuffers.resize(m_Swapchain->GetImageViews().size());
        for (size_t i = 0; i < m_Swapchain->GetImageViews().size(); i++) {
            std::array<VkImageView, 2> attachments = {m_Swapchain->GetImageViews()[i], m_DepthImageView};
            VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; /* ... setup ... */
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_Swapchain->GetExtent().width;
            framebufferInfo.height = m_Swapchain->GetExtent().height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(m_VulkanContext->device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]));
        }
        VKENG_INFO("{} Framebuffers Created.", m_SwapChainFramebuffers.size());
    }

    void Renderer::CreateUniformBuffers() { /* ... As in previous "Create Light Uniform Buffers" section ... */
        VKENG_INFO("Creating Camera Uniform Buffers ({})...", MAX_FRAMES_IN_FLIGHT);
        VkDeviceSize bufferSize = sizeof(CameraMatricesUBO);
        m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_UniformBuffers[i] = std::make_unique<VulkanBuffer>(
                *m_VulkanContext, bufferSize, 1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK(m_UniformBuffers[i]->Map());
        }
    }
    void Renderer::CreateLightUniformBuffers() { /* ... As in previous "Create Light Uniform Buffers" section ... */
        VKENG_INFO("Creating Light Uniform Buffers ({})...", MAX_FRAMES_IN_FLIGHT);
        VkDeviceSize bufferSize = sizeof(LightDataUBO);
        m_LightUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_LightUniformBuffers[i] = std::make_unique<VulkanBuffer>(
                *m_VulkanContext, bufferSize, 1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK(m_LightUniformBuffers[i]->Map());
        }
    }

    void Renderer::CreateDescriptorPool() { /* ... As in previous "Create Descriptor Pool" for Frame + Material ... */
        VKENG_INFO("Creating Descriptor Pool (Frame + Material)...");
        std::vector<VkDescriptorPoolSize> poolSizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2)}, // Camera + Light
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000} // For materials
        };
        uint32_t maxTotalSets = MAX_FRAMES_IN_FLIGHT + 1000;
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxTotalSets;
        VK_CHECK(vkCreateDescriptorPool(m_VulkanContext->device, &poolInfo, nullptr, &m_DescriptorPool));
        VKENG_INFO("Descriptor Pool Created.");
    }

    void Renderer::CreateFrameDescriptorSets() { /* ... As in previous "CreateFrameDescriptorSets" for Camera + Light UBOs ... */
        VKENG_INFO("Creating Frame Descriptor Sets (Set 0 - Camera UBO + Light UBO)...");
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_FrameDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
        m_FrameDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(m_VulkanContext->device, &allocInfo, m_FrameDescriptorSets.data()));

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo cameraInfo = m_UniformBuffers[i]->DescriptorInfo(sizeof(CameraMatricesUBO));
            VkDescriptorBufferInfo lightInfo = m_LightUniformBuffers[i]->DescriptorInfo(sizeof(LightDataUBO));
            std::array<VkWriteDescriptorSet, 2> writes{};
            writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; /* ... setup for camera UBO binding 0 ... */
            writes[0].dstSet = m_FrameDescriptorSets[i]; writes[0].dstBinding = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1; writes[0].pBufferInfo = &cameraInfo;
            writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; /* ... setup for light UBO binding 1 ... */
            writes[1].dstSet = m_FrameDescriptorSets[i]; writes[1].dstBinding = 1; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[1].descriptorCount = 1; writes[1].pBufferInfo = &lightInfo;
            vkUpdateDescriptorSets(m_VulkanContext->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
        VKENG_INFO("Frame Descriptor Sets Updated.");
    }


    // --- Per-Frame Updates ---
    void Renderer::UpdateCameraUBO(uint32_t currentFrameIndex, const glm::mat4& view, const glm::mat4& proj) {
        CameraMatricesUBO ubo{view, proj};
        m_UniformBuffers[currentFrameIndex]->WriteToBuffer(&ubo, sizeof(ubo));
    }
    void Renderer::UpdateLightUBO(uint32_t currentFrameIndex) {
        LightDataUBO ubo{};
        ubo.direction = glm::vec4(m_LightDirection, 0.0f);
        ubo.color = glm::vec4(m_LightColor * m_LightIntensity, m_LightIntensity); // Store intensity in alpha too
        m_LightUniformBuffers[currentFrameIndex]->WriteToBuffer(&ubo, sizeof(ubo));
    }

    // --- Shader Loading Helpers ---
    std::vector<char> Renderer::ReadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("failed to open file: " + filename);
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }
    VkShaderModule Renderer::CreateShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(m_VulkanContext->device, &createInfo, nullptr, &shaderModule));
        return shaderModule;
    }

} // namespace VulkEng
