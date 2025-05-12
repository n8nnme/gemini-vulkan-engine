#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec3 fragPosWorld;

// Descriptor Set 0: Frame Data (Bound once per frame)
layout(set = 0, binding = 1) uniform LightData { // Binding 1 for Light
    vec4 direction; // Directional light direction (FROM light source to origin)
    vec4 color;     // Light color (rgb) + intensity (a)
} lightData;

// Descriptor Set 1: Material Data (Bound per material)
layout(set = 1, binding = 0) uniform sampler2D texSampler; // Binding 0 for Diffuse Texture

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Surface Properties
    vec4 albedoSample = texture(texSampler, fragTexCoord);
    vec3 surfaceAlbedo = albedoSample.rgb * fragColor.rgb; // Modulate texture by vertex color
    float surfaceAlpha = albedoSample.a * fragColor.a;
    vec3 N = normalize(fragNormalWorld); // Normalized surface normal

    // Lighting
    // Ambient term
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * lightData.color.rgb;

    // Diffuse term (Lambertian)
    vec3 L = normalize(-lightData.direction.xyz); // Vector TO the light source
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuseLightColor = lightData.color.rgb * lightData.color.a; // Light color * intensity
    vec3 diffuse = NdotL * diffuseLightColor;

    // TODO: Add Specular term for more realistic lighting (e.g., Blinn-Phong or PBR Cook-Torrance)
    // vec3 V = normalize(cameraData.cameraPosition_worldSpace - fragPosWorld); // Vector TO the viewer/camera
    // vec3 R = reflect(-L, N);
    // float spec = pow(max(dot(V, R), 0.0), material.shininess); // Example Blinn-Phong
    // vec3 specular = material.specularColor * spec * diffuseLightColor;

    // Combine lighting components
    vec3 finalColor = (ambient + diffuse /* + specular */) * surfaceAlbedo;

    // Output final color
    outColor = vec4(finalColor, surfaceAlpha);

    // Optional Gamma Correction (if swapchain is UNORM and linear workflow is used)
    // if using SRGB swapchain format, this is not needed as hardware handles it.
    // outColor.rgb = pow(outColor.rgb, vec3(1.0/2.2));
}
