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

layout(location = 0) in vec3 inViewPos;
layout(location = 1) flat in int inLineIndex;
layout(location = 0) out vec4 outColor;

// Ray-capsule intersection in view space (camera at the origin, segment
// [start, end], radius = thickness / 2): cylinder body + two hemispherical
// end caps. Returns the nearest hit t, or a negative number on a miss.
float intersectLine(vec3 rayDir, vec3 start, vec3 end, float radius) {
    vec3 segment = end - start;
    vec3 oa = -start; // rayOrigin (0,0,0) - start

    float segLenSq = dot(segment, segment);
    float segDotDir = dot(segment, rayDir);
    float segDotOa = dot(segment, oa);
    float dirDotOa = dot(rayDir, oa);
    float oaLenSq = dot(oa, oa);

    float a = segLenSq - segDotDir * segDotDir;
    float b = segLenSq * dirDotOa - segDotOa * segDotDir;
    float c = segLenSq * oaLenSq - segDotOa * segDotOa - radius * radius * segLenSq;
    float h = b * b - a * c;
    if (h < 0.0) {
        return -1.0;
    }
    h = sqrt(h);
    float t = (-b - h) / a;
    float y = segDotOa + t * segDotDir; // hit's projection onto the segment, in [0, segLenSq]

    if (y > 0.0 && y < segLenSq) {
        return t; // hit the cylindrical body
    }

    // Missed the body: test whichever endpoint cap the ray is nearer to.
    vec3 capCenter = (y <= 0.0) ? start : end;
    vec3 oc = -capCenter;
    float capB = dot(rayDir, oc);
    float capC = dot(oc, oc) - radius * radius;
    float capH = capB * capB - capC;
    if (capH < 0.0) {
        return -1.0;
    }
    return -capB - sqrt(capH);
}

vec3 closestPointOnSegment(vec3 point, vec3 start, vec3 end) {
    vec3 segment = end - start;
    // max(): a zero-length segment would otherwise give 0/0 and a NaN normal.
    float t = clamp(dot(point - start, segment) / max(dot(segment, segment), 1e-12), 0.0, 1.0);
    return start + segment * t;
}

void main() {
    Line line = lines.data[inLineIndex];
    float radius = max(line.thickness * 0.5, 0.0);

    vec3 startView = (uniforms.camera.view * vec4(line.start, 1.0)).xyz;
    vec3 endView = (uniforms.camera.view * vec4(line.end, 1.0)).xyz;

    vec3 rayDir = normalize(inViewPos);
    float t = intersectLine(rayDir, startView, endView, radius);
    if (t <= 0.0) {
        discard;
    }

    vec3 hitView = rayDir * t;
    vec3 normalView = normalize(hitView - closestPointOnSegment(hitView, startView, endView));

    vec3 baseColor = line.color.rgb; // alpha unused: no blending on this pipeline

    vec3 lightDirView = normalize(mat3(uniforms.camera.view) * vec3(0.4, 0.8, 0.5));
    float diffuse = max(dot(normalView, lightDirView), 0.0);
    outColor = vec4(baseColor * (0.35 + 0.65 * diffuse), 1.0);

    vec4 clipPos = uniforms.camera.projection * vec4(hitView, 1.0);
    gl_FragDepth = clipPos.z / clipPos.w;
}
