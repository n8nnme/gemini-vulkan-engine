#pragma once

// --- Base Service Class Headers ---
// These are the actual interfaces/classes our services will be.
#include "graphics/Renderer.h"
#include "assets/AssetManager.h"
#include "physics/PhysicsSystem.h"
#include "ui/UIManager.h"
#include "core/Log.h" // For logging within Null services

// --- Headers for Dummy Base Objects (for Null Service Constructors) ---
// These are required because the Null services inherit from concrete classes
// that have constructor dependencies. Ideally, use interfaces.
#include "core/Window.h"
#include "graphics/VulkanContext.h"
#include "graphics/CommandManager.h"

#include <stdexcept> // For std::runtime_error
#include <cstddef>   // For nullptr_t (though not strictly needed with inline static init)

namespace VulkEng {

    // --- Minimal Dummy Implementations for Base Class Dependencies ---
    // These exist SOLELY to allow Null service object constructors to compile.
    // They assume the base classes (Window, VulkanContext, CommandManager, UIManager)
    // have been modified to include a 'bool skipInit = false' constructor parameter.
    // THIS IS A WORKAROUND FOR NOT USING INTERFACES.

    class DummyWindowSL : public Window {
    public:
        DummyWindowSL() : Window(1, 1, "DummySL_Window", true) {} // true = skip GLFW init
        void PollEvents() override { /* No-op */ }
        bool ShouldClose() const override { return true; }
        void GetFramebufferSize(int& w, int& h) const override { w = 1; h = 1; }
        void CreateWindowSurface(VkInstance, VkSurfaceKHR*) override { /* No-op */ }
        GLFWwindow* GetGLFWwindow() const override { return nullptr; }
        // Add other necessary virtual overrides from Window if any
    };

    class DummyVulkanContextSL : public VulkanContext {
    public:
        DummyVulkanContextSL(Window& w) : VulkanContext(w, true) {} // true = skip Vulkan init
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice) override { return {}; }
        SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice) override { return {}; }
        // Add other necessary virtual overrides from VulkanContext if any
    };

    class DummyCommandManagerSL : public CommandManager {
    public:
        DummyCommandManagerSL(VulkanContext& ctx, uint32_t imgCount) : CommandManager(ctx, imgCount, true) {} // true = skip Vulkan init
        VkCommandBuffer BeginFrame(uint32_t) override { return VK_NULL_HANDLE; }
        void EndFrameRecording(uint32_t) override { /* No-op */ } // Matched renamed method
        VkCommandPool GetCommandPool() const override { return VK_NULL_HANDLE; }
        // SingleTimeCommands are now helpers in VulkanContext/Utils, not part of CommandManager interface
        // Add other necessary virtual overrides from CommandManager if any
    };


    // --- Null Service Class Implementations ---

    class NullRenderer : public Renderer {
    public:
        NullRenderer() : Renderer(m_NullWindowForServices, m_NullContextForServices) {
            VKENG_WARN_ONCE("NullRenderer instance created. Rendering will not function.");
        }
        bool BeginFrame() override { return false; }
        void RecordCommands(const std::vector<RenderObjectInfo>&, CameraComponent*) override {}
        void EndFrameAndPresent() override {}
        void HandleResize(int, int) override {}
        void WaitForDeviceIdle() override {}
        VulkanContext& GetContext() override { return m_NullContextForServices; }
        VkCommandBuffer GetCurrentCommandBuffer() override { return VK_NULL_HANDLE; }
        VkRenderPass GetMainRenderPass() const override { return VK_NULL_HANDLE; }
        CommandManager& GetCommandManagerInstance() override { return m_NullCommandManagerForServices; }
    private:
        // These static dummies are shared among Null services that need them for construction
        inline static DummyWindowSL m_NullWindowForServices;
        inline static DummyVulkanContextSL m_NullContextForServices{m_NullWindowForServices};
        inline static DummyCommandManagerSL m_NullCommandManagerForServices{m_NullContextForServices, 0};
    };

    class NullAssetManager : public AssetManager {
    public:
         NullAssetManager() : AssetManager(NullRenderer::m_NullContextForServices, NullRenderer::m_NullCommandManagerForServices) {
             VKENG_WARN_ONCE("NullAssetManager instance created. Asset loading will not function.");
         }
        ModelHandle LoadModel(const std::string&) override { return InvalidModelHandle; }
        const std::vector<Mesh>& GetModelMeshes(ModelHandle) const override { static const std::vector<Mesh> e; return e; }
        TextureHandle LoadTexture(const std::string&, bool) override { return InvalidTextureHandle; }
        const Texture& GetTexture(TextureHandle) const override { static Texture t; return t; }
        const Material& GetMaterial(MaterialHandle) const override { static Material m; return m; }
        const LoadedModelData* GetLoadedModelData(ModelHandle) const override { return nullptr; }
        TextureHandle GetDefaultWhiteTexture() const override { return InvalidTextureHandle; }
        MaterialHandle GetDefaultMaterial() const override { return InvalidMaterialHandle; }
    };

    class NullPhysicsSystem : public PhysicsSystem {
    public:
        NullPhysicsSystem() { VKENG_WARN_ONCE("NullPhysicsSystem instance created. Physics will not function."); }
        void Update(float, int, float) override {}
        void AddRigidBody(btRigidBody*) override {}
        void RemoveRigidBody(btRigidBody*) override {}
        btDynamicsWorld* GetWorld() const override { return nullptr; }
    };

    class NullUIManager : public UIManager {
    public:
        NullUIManager() : UIManager(NullRenderer::m_NullWindowForServices, NullRenderer::m_NullContextForServices, VK_NULL_HANDLE, true) {
            VKENG_WARN_ONCE("NullUIManager instance created. UI will not function.");
        }
        void BeginUIRender() override {}
        void EndUIRender() override {}
        void RenderDrawData(VkCommandBuffer) override {}
    };


    // --- ServiceLocator Class ---
    class ServiceLocator {
    public:
        // Deleted constructor/destructor for a static class
        ServiceLocator() = delete;
        ~ServiceLocator() = delete;

        // --- Provide Methods ---
        // Call these during application initialization *after* the systems are created.
        static void Provide(Renderer* renderer) {
            s_Renderer = (renderer == nullptr) ? &m_StaticNullRenderer : renderer;
        }
        static void Provide(AssetManager* assets) {
             s_AssetManager = (assets == nullptr) ? &m_StaticNullAssetManager : assets;
         }
         static void Provide(PhysicsSystem* physics) {
              s_PhysicsSystem = (physics == nullptr) ? &m_StaticNullPhysicsSystem : physics;
          }
        static void Provide(UIManager* ui) {
            s_UIManager = (ui == nullptr) ? &m_StaticNullUIManager : ui;
        }
        // Add Provide methods for other services...


        // --- Get Methods ---
        // Use these anywhere in the codebase to access the services.
        // Returns a reference, guaranteeing a non-null object (either real or Null version).
        static Renderer& GetRenderer() { return *s_Renderer; }
        static AssetManager& GetAssetManager() { return *s_AssetManager; }
        static PhysicsSystem& GetPhysicsSystem() { return *s_PhysicsSystem; }
        static UIManager& GetUIManager() { return *s_UIManager; }
        // Add Get methods for other services...


        // --- Reset Method ---
        // Call during application shutdown to revert services to their Null implementations.
         static void Reset() {
              s_Renderer = &m_StaticNullRenderer;
              s_AssetManager = &m_StaticNullAssetManager;
              s_PhysicsSystem = &m_StaticNullPhysicsSystem;
              s_UIManager = &m_StaticNullUIManager;
              // Reset other services...
              VKENG_INFO("ServiceLocator: Services reset to Null implementations.");
         }

    private:
        // --- Static Null Service Object Instances ---
        // These are the objects returned if a real service isn't provided or after Reset().
        inline static NullRenderer m_StaticNullRenderer;
        inline static NullAssetManager m_StaticNullAssetManager;
        inline static NullPhysicsSystem m_StaticNullPhysicsSystem;
        inline static NullUIManager m_StaticNullUIManager;
        // Add instances for other Null services...

        // --- Static Service Pointers ---
        // These point to either the real service instance or a Null service instance.
        // Initialize them to point to the Null objects by default.
        inline static Renderer* s_Renderer = &m_StaticNullRenderer;
        inline static AssetManager* s_AssetManager = &m_StaticNullAssetManager;
        inline static PhysicsSystem* s_PhysicsSystem = &m_StaticNullPhysicsSystem;
        inline static UIManager* s_UIManager = &m_StaticNullUIManager;
        // Add pointers for other services...
    };

} // namespace VulkEng




















// 12 May 19:02 | ???
// #pragma once
// 
// // Base Service Class Headers
// #include "graphics/Renderer.h"
// #include "assets/AssetManager.h"
// #include "physics/PhysicsSystem.h"
// #include "ui/UIManager.h"
// #include "core/Log.h"
// 
// // For Dummy Objects (which are problematic)
// #include "core/Window.h"
// #include "graphics/VulkanContext.h"
// #include "graphics/CommandManager.h"
// 
// #include <stdexcept>
// #include <cstddef>
// 
// namespace VulkEng {
// 
//     // --- Minimal Dummy Implementations (INDICATES BASE CLASS DESIGN ISSUES!) ---
//     // These assume base classes have constructors with 'bool skipInit = false'
//     class DummyWindowSL : public Window { // Renamed to avoid conflict if DummyWindow is also a standalone file
//     public: DummyWindowSL() : Window(1, 1, "DummySL", true) {}
//         void PollEvents() override {} /* etc. */
//         bool ShouldClose() const override { return true;}
//         void GetFramebufferSize(int&w, int&h) const override {w=1;h=1;}
//         void CreateWindowSurface(VkInstance, VkSurfaceKHR*) override {}
//         GLFWwindow* GetGLFWwindow() const override {return nullptr;}
//     };
//     class DummyVulkanContextSL : public VulkanContext {
//     public: DummyVulkanContextSL(Window& w) : VulkanContext(w, true) {}
//         QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice) override { return {}; }
//         SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice) override { return {}; }
//     };
//     class DummyCommandManagerSL : public CommandManager {
//     public: DummyCommandManagerSL(VulkanContext& ctx, uint32_t c) : CommandManager(ctx, c, true) {}
//         VkCommandBuffer BeginFrame(uint32_t) override { return VK_NULL_HANDLE; }
//         void EndFrame(uint32_t) override {}
//         VkCommandPool GetCommandPool() const override {return VK_NULL_HANDLE;}
//         VkCommandBuffer BeginSingleTimeCommands() override {return VK_NULL_HANDLE;}
//         void EndSingleTimeCommands(VkCommandBuffer) override {}
//     };
// 
// 
//     // --- Null Service Classes ---
//     class NullRenderer : public Renderer {
//     public:
//         NullRenderer() : Renderer( m_NullWindow, m_NullContext ) { VKENG_WARN("NullRenderer created."); }
//         bool BeginFrame() override { return false; }
//         void RecordCommands(const std::vector<RenderObjectInfo>&, CameraComponent*) override {}
//         void EndFrameAndPresent() override {}
//         void HandleResize(int, int) override {}
//         void WaitForDeviceIdle() override {}
//         VulkanContext& GetContext() override { return m_NullContext; }
//         VkCommandBuffer GetCurrentCommandBuffer() override { return VK_NULL_HANDLE; }
//         VkRenderPass GetMainRenderPass() const override { return VK_NULL_HANDLE; }
//         CommandManager& GetCommandManagerInstance() override { return m_NullCommandManager; } // Add override
//     private:
//         inline static DummyWindowSL m_NullWindow;
//         inline static DummyVulkanContextSL m_NullContext{m_NullWindow};
//         inline static DummyCommandManagerSL m_NullCommandManager{m_NullContext, 0}; // Add dummy command manager
//     };
// 
//     class NullAssetManager : public AssetManager {
//     public:
//          NullAssetManager() : AssetManager( NullRenderer::m_NullContext, NullRenderer::m_NullCommandManager ) { VKENG_WARN("NullAssetManager created."); }
//         ModelHandle LoadModel(const std::string&) override { return InvalidModelHandle; }
//         const std::vector<Mesh>& GetModelMeshes(ModelHandle) const override { static const std::vector<Mesh> e; return e; }
//         TextureHandle LoadTexture(const std::string&, bool) override { return InvalidTextureHandle; }
//         const Texture& GetTexture(TextureHandle) const override { static Texture t; return t; }
//         const Material& GetMaterial(MaterialHandle) const override { static Material m; return m; }
//         const LoadedModelData* GetLoadedModelData(ModelHandle) const override { return nullptr; }
//         TextureHandle GetDefaultWhiteTexture() const override { return InvalidTextureHandle; }
//         MaterialHandle GetDefaultMaterial() const override { return InvalidMaterialHandle; }
//     };
// 
//     class NullPhysicsSystem : public PhysicsSystem {
//     public:
//         NullPhysicsSystem() { VKENG_WARN("NullPhysicsSystem created."); }
//         void Update(float, int, float) override {}
//         void AddRigidBody(btRigidBody*) override {}
//         void RemoveRigidBody(btRigidBody*) override {}
//         btDynamicsWorld* GetWorld() const override { return nullptr; }
//     };
// 
//     class NullUIManager : public UIManager {
//     public:
//         NullUIManager() : UIManager(NullRenderer::m_NullWindow, NullRenderer::m_NullContext, VK_NULL_HANDLE, true) { VKENG_WARN("NullUIManager created."); }
//         void BeginUIRender() override {}
//         void EndUIRender() override {}
//         void RenderDrawData(VkCommandBuffer) override {}
//     };
// 
// 
//     // --- ServiceLocator ---
//     class ServiceLocator {
//     public:
//         static void Provide(Renderer* renderer) { s_Renderer = (renderer == nullptr) ? &m_NullRenderer : renderer; }
//         static void Provide(AssetManager* assets) { s_AssetManager = (assets == nullptr) ? &m_NullAssetManager : assets; }
//         static void Provide(PhysicsSystem* physics) { s_PhysicsSystem = (physics == nullptr) ? &m_NullPhysicsSystem : physics; }
//         static void Provide(UIManager* ui) { s_UIManager = (ui == nullptr) ? &m_NullUIManager : ui; }
// 
//         static Renderer& GetRenderer() { return *s_Renderer; }
//         static AssetManager& GetAssetManager() { return *s_AssetManager; }
//         static PhysicsSystem& GetPhysicsSystem() { return *s_PhysicsSystem; }
//         static UIManager& GetUIManager() { return *s_UIManager; }
// 
//         static void Reset() {
//               s_Renderer = &m_NullRenderer;
//               s_AssetManager = &m_NullAssetManager;
//               s_PhysicsSystem = &m_NullPhysicsSystem;
//               s_UIManager = &m_NullUIManager;
//          }
//     private:
//         inline static NullRenderer m_NullRenderer;
//         inline static NullAssetManager m_NullAssetManager;
//         inline static NullPhysicsSystem m_NullPhysicsSystem;
//         inline static NullUIManager m_NullUIManager;
// 
//         inline static Renderer* s_Renderer = &m_NullRenderer;
//         inline static AssetManager* s_AssetManager = &m_NullAssetManager;
//         inline static PhysicsSystem* s_PhysicsSystem = &m_NullPhysicsSystem;
//         inline static UIManager* s_UIManager = &m_NullUIManager;
//     };
// }
