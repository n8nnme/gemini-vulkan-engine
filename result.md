VulkanEngine/
├── src/
│   ├── core/
│   │   ├── Application.h/.cpp
│   │   ├── Engine.h/.cpp        (We haven't focused on this, can be minimal wrapper)
│   │   ├── Window.h/.cpp
│   │   ├── InputManager.h/.cpp
│   │   ├── Log.h/.cpp
│   │   └── ServiceLocator.h     (Contains NullServices.h content for simplicity here)
│   ├── graphics/
│   │   ├── Renderer.h/.cpp
│   │   ├── VulkanContext.h/.cpp
│   │   ├── Swapchain.h/.cpp
│   │   ├── Buffer.h/.cpp        (VulkanBuffer class)
│   │   ├── CommandManager.h/.cpp
│   │   ├── SamplerCache.h/.cpp
│   │   └── VulkanUtils.h/.cpp
│   ├── scene/
│   │   ├── Scene.h/.cpp
│   │   ├── GameObject.h/.cpp
│   │   ├── Component.h
│   │   ├── Components/          (Directory for specific components)
│   │   │   ├── TransformComponent.h/.cpp
│   │   │   ├── MeshComponent.h/.cpp
│   │   │   ├── CameraComponent.h/.cpp
│   │   │   └── RigidBodyComponent.h/.cpp
│   ├── assets/
│   │   ├── AssetManager.h/.cpp
│   │   ├── Mesh.h/.cpp
│   │   ├── Material.h
│   │   ├── Texture.h
│   │   └── ModelLoader.h/.cpp
│   ├── ui/
│   │   └── UIManager.h/.cpp
│   ├── physics/
│   │   ├── PhysicsSystem.h/.cpp
│   │   ├── EngineMotionState.h
│   │   └── CollisionCallbackReceiver.h
│   │   └── CustomTickCallback.h/.cpp
│   └── main.cpp
├── external/             (For libraries like GLM, ImGui, Assimp, stb_image, Bullet)
├── assets/
│   ├── models/             (Place .obj/.fbx files here, e.g., viking_room.obj)
│   │   └── textures/       (Place textures here if not embedded or alongside model)
│   └── shaders/
│       ├── simple.vert
│       └── simple.frag
├── CMakeLists.txt
└── README.md
