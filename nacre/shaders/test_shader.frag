#version 450

struct Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
};

struct ObjectData {
    mat4 model;
    vec4 color;
};

layout(set = 0, binding = 0) uniform UniformData {
    Camera camera;
} uniforms;

layout(set = 1, binding = 0) uniform Material {
    vec4 color;
} material;

layout(set = 2, binding = 0) readonly buffer Object {
    ObjectData[] data;
} objects;

layout(push_constant) uniform PushConstants {
    uint id;
} pc;

layout(location = 0) in vec3 inNormal; // world space
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Same fixed world-space light and ambient/diffuse split as the
    // visualisation (ball/line) shaders, so everything is lit consistently.
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.5));
    float diffuse = max(dot(normal, lightDir), 0.0);

    vec4 objectColor = objects.data[pc.id].color;
    vec3 baseColor = material.color.rgb * objectColor.rgb * inColor;
    outColor = vec4(baseColor * (0.35 + 0.65 * diffuse), material.color.a * objectColor.a);
}