#version 450

struct Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
};

// Must match DrawEntry::Line (std430: start@0, end@16, thickness@28,
// color@32, stride 48).
struct Line {
    vec3 start;
    vec3 end;
    float thickness;
    vec4 color;
};

layout(set = 0, binding = 0) uniform UniformData {
    Camera camera;
} uniforms;

layout(set = 1, binding = 1) readonly buffer Lines {
    Line data[];
} lines;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) flat out int outLineIndex;

// The 12 triangles of the box [-1,1]^3, from gl_VertexIndex alone (no
// vertex/index buffer). Corner bits: 1 = +x, 2 = +y, 4 = +z.
vec3 boxCorner(int vertexIndex) {
    const int corners[36] = int[](0, 2, 6, 0, 6, 4,  // -x
                                  1, 5, 7, 1, 7, 3,  // +x
                                  0, 4, 5, 0, 5, 1,  // -y
                                  2, 3, 7, 2, 7, 6,  // +y
                                  0, 1, 3, 0, 3, 2,  // -z
                                  4, 6, 7, 4, 7, 5); // +z
    int c = corners[vertexIndex];
    return vec3((c & 1) != 0 ? 1.0 : -1.0, (c & 2) != 0 ? 1.0 : -1.0,
                (c & 4) != 0 ? 1.0 : -1.0);
}

void main() {
    Line line = lines.data[gl_InstanceIndex];
    float radius = max(line.thickness * 0.5, 0.0);

    vec3 startView = (uniforms.camera.view * vec4(line.start, 1.0)).xyz;
    vec3 endView = (uniforms.camera.view * vec4(line.end, 1.0)).xyz;

    // An oriented box around the capsule: it covers the capsule's silhouette
    // from any angle (end-on included), and endpoints behind the camera are
    // handled by ordinary near-plane clipping. The fragment shader ray-tests
    // the exact capsule. No culling, so a pixel may shade both a front and a
    // back face; both give the same hit.
    vec3 axis = endView - startView;
    float axisLen = length(axis);
    vec3 axisDir = axisLen > 1e-6 ? axis / axisLen : vec3(1.0, 0.0, 0.0);
    vec3 helper = abs(axisDir.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 u = normalize(cross(axisDir, helper));
    vec3 v = cross(axisDir, u);

    vec3 corner = boxCorner(gl_VertexIndex);
    vec3 cornerView = (startView + endView) * 0.5 + axisDir * (corner.x * (axisLen * 0.5 + radius)) +
                      u * (corner.y * radius) + v * (corner.z * radius);

    outViewPos = cornerView;
    outLineIndex = gl_InstanceIndex;

    gl_Position = uniforms.camera.projection * vec4(cornerView, 1.0);
}
