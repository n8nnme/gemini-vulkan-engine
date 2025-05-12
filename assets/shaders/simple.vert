#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec3 inTangent;

// Descriptor Set 0: Frame Data (Bound once per frame)
layout(set = 0, binding = 0) uniform CameraMatrices {
    mat4 view;
    mat4 proj;
} cameraData;
// LightData UBO (set = 0, binding = 1) is used only in Fragment shader

// Push Constants: Per-Object Data
layout(push_constant) uniform PushConstants {
    mat4 model; // Model-to-World matrix
} pushConsts;

// Output to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormalWorld; // Normal in world space
layout(location = 3) out vec3 fragPosWorld;   // Position in world space

void main() {
    vec4 worldPos = pushConsts.model * vec4(inPosition, 1.0);
    fragPosWorld = worldPos.xyz;

    gl_Position = cameraData.proj * cameraData.view * worldPos;

    // Approximation for normal matrix (works for uniform scale/rotation)
    mat3 normalMatrix = mat3(pushConsts.model);
    // For non-uniform scaling, use: transpose(inverse(mat3(pushConsts.model)))
    fragNormalWorld = normalize(normalMatrix * inNormal);

    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
