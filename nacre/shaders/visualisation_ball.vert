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

layout(location = 0) out vec3 outViewPos;
layout(location = 1) flat out int outBallIndex;

// Two triangles covering a [-1,1]x[-1,1] quad, no vertex/index buffer needed.
vec2 quadCorner(int vertexIndex) {
    vec2 corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
                             vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
    return corners[vertexIndex];
}

void main() {
    Ball ball = balls.data[gl_InstanceIndex];
    float radius = max(ball.radius, 0.0);

    vec3 centerView = (uniforms.camera.view * vec4(ball.position, 1.0)).xyz;

    // A flat quad offset by exactly `radius` at the center's own depth
    // undersizes the sphere under perspective: the camera's tangent lines
    // touch the sphere at angle asin(radius/dist), but the flat quad only
    // spans atan(radius/dist), and asin(x) > atan(x) for any x > 0 -- so the
    // quad always clips the silhouette a little, worse the closer the
    // camera gets. Scale the billboard radius so its angular half-width
    // matches the tangent cone's angle exactly.
    float dist = length(centerView);
    float safeDist = max(dist, radius + 1e-4); // guard: camera inside the sphere
    float billboardRadius =
        radius * safeDist / sqrt(max(safeDist * safeDist - radius * radius, 1e-8));

    // Orient the quad perpendicular to the eye->center ray, not to the view
    // axis. That plane cuts the tangent cone in a true circle of radius
    // billboardRadius at the center, so coverage is exact anywhere on screen.
    // A view-axis-aligned quad would cut the cone in an ellipse stretched
    // away from screen center (~1/cos of the off-axis angle), clipping
    // off-center balls on their outer edge.
    vec3 w = centerView / max(dist, 1e-6);
    vec3 up = abs(w.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, w));
    vec3 upv = cross(w, right);

    vec2 corner = quadCorner(gl_VertexIndex);
    vec3 cornerView = centerView + (right * corner.x + upv * corner.y) * billboardRadius;

    outViewPos = cornerView;
    outBallIndex = gl_InstanceIndex;

    gl_Position = uniforms.camera.projection * vec4(cornerView, 1.0);
}
