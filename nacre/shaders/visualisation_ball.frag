#version 450

struct Camera {
    mat4 view;
    mat4 projection;
    vec4 position;
};

struct Ball {
    vec3 position;
    float radius;
};

layout(set = 0, binding = 0) uniform UniformData {
    Camera camera;
} uniforms;

layout(set = 1, binding = 0) readonly buffer Balls {
    Ball data[];
} balls;

layout(push_constant) uniform PushConstants {
    float minRadius;
    float maxRadius;
} pc;

layout(location = 0) in vec3 inViewPos;
layout(location = 1) flat in int inBallIndex;
layout(location = 0) out vec4 outColor;

// Ray-sphere intersection in view space (camera at the origin). Returns the
// nearest positive t, or a negative number if the ray misses.
float intersectBall(vec3 rayDir, vec3 centerView, float radius) {
    vec3 oc = -centerView; // rayOrigin (0,0,0) - centerView
    float b = dot(oc, rayDir);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - c;
    if (discriminant < 0.0) {
        return -1.0;
    }
    float sq = sqrt(discriminant);
    float tNear = -b - sq;
    float tFar = -b + sq;
    return tNear > 0.0 ? tNear : tFar;
}

void main() {
    Ball ball = balls.data[inBallIndex];
    float radius = max(ball.radius, 0.0);
    vec3 centerView = (uniforms.camera.view * vec4(ball.position, 1.0)).xyz;

    // Camera is the origin in view space, so this is the ray straight from
    // the eye through this fragment's point on the billboard plane.
    vec3 rayDir = normalize(inViewPos);
    float t = intersectBall(rayDir, centerView, radius);
    if (t <= 0.0) {
        discard;
    }

    vec3 hitView = rayDir * t;
    vec3 normalView = normalize(hitView - centerView);

    float weightRange = max(pc.maxRadius - pc.minRadius, 1e-5);
    float w = clamp((radius - pc.minRadius) / weightRange, 0.0, 1.0);
    vec3 lowColor = vec3(0.15, 0.35, 1.0);
    vec3 highColor = vec3(1.0, 0.25, 0.1);
    vec3 baseColor = mix(lowColor, highColor, w);

    vec3 lightDirView = normalize(mat3(uniforms.camera.view) * vec3(0.4, 0.8, 0.5));
    float diffuse = max(dot(normalView, lightDirView), 0.0);
    outColor = vec4(baseColor * (0.35 + 0.65 * diffuse), 1.0);

    vec4 clipPos = uniforms.camera.projection * vec4(hitView, 1.0);
    gl_FragDepth = clipPos.z / clipPos.w;
}
