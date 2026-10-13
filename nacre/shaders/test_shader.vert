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

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 outNormal; // world space
layout(location = 1) out vec3 outColor;

void main() {
    mat4 model = objects.data[pc.id].model;
    // mat3(model) is only correct for rotation + uniform scale; switch to the
    // inverse-transpose if non-uniform scaling ever shows up.
    outNormal = mat3(model) * inNormal;
    outColor = inColor;
    gl_Position = uniforms.camera.projection * uniforms.camera.view * model * vec4(inPos, 1.0);
}