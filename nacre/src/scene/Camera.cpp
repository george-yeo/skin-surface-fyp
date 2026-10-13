#include <nacre/scene/Camera.hpp>

namespace nacre::scene {

Camera::Camera(glm::vec3 origin, glm::vec3 forward, glm::vec3 up, float nearPlane,
               float farPlane) noexcept
    : origin(origin),
      rotation(glm::quatLookAt(forward, up)),
      forward(forward),
      up(up),
      nearPlane(nearPlane),
      farPlane(farPlane),
      initialOrigin(origin),
      initialForward(forward),
      initialUp(up) {}

void Camera::resetPose() noexcept {
    origin = initialOrigin;
    forward = initialForward;
    up = initialUp;
    rotation = glm::quatLookAt(forward, up);
}

void Camera::setForward(glm::vec3 f) noexcept {
    forward = f;
    rotation = glm::quatLookAt(forward, up);
}

void Camera::setUp(glm::vec3 u) noexcept {
    up = u;
    rotation = glm::quatLookAt(forward, up);
}

void Camera::translate(glm::vec3 translation) noexcept {
    origin += rotation * translation;
}

void Camera::rotate(glm::vec3 rot) noexcept {
    glm::quat yaw = glm::angleAxis(glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat pitch = glm::angleAxis(glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat roll = glm::angleAxis(glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));

    rotation = glm::normalize(yaw * rotation * pitch * roll);

    forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
}

std::pair<glm::vec3, glm::vec3> Camera::screenRay(glm::vec2 ndc) const noexcept {
    glm::mat4 invViewProj = glm::inverse(projection() * view());

    // Depth is [0, 1] (GLM_FORCE_DEPTH_ZERO_TO_ONE): 0 = near, 1 = far.
    glm::vec4 nearH = invViewProj * glm::vec4(ndc, 0.0f, 1.0f);
    glm::vec4 farH = invViewProj * glm::vec4(ndc, 1.0f, 1.0f);

    glm::vec3 nearPoint = glm::vec3(nearH) / nearH.w;
    glm::vec3 farPoint = glm::vec3(farH) / farH.w;
    return {nearPoint, farPoint - nearPoint};
}

std::optional<glm::vec3> Camera::unprojectToViewPlane(glm::vec2 ndc,
                                                      float distance) const noexcept {
    const auto [nearPoint, rayDir] = screenRay(ndc);

    const glm::vec3 normal = glm::normalize(forward);
    const float denom = glm::dot(rayDir, normal);
    if (glm::abs(denom) < 1e-6f) {
        return std::nullopt;
    }

    const glm::vec3 planePoint = origin + normal * distance;
    const float t = glm::dot(planePoint - nearPoint, normal) / denom;
    return nearPoint + t * rayDir;
}

std::optional<glm::vec3> Camera::unprojectToPlane(glm::vec2 ndc, float planeZ) const noexcept {
    const auto [nearPoint, rayDir] = screenRay(ndc);

    if (glm::abs(rayDir.z) < 1e-6f) {
        return std::nullopt;
    }

    float t = (planeZ - nearPoint.z) / rayDir.z;
    return nearPoint + t * rayDir;
}

} // namespace nacre::scene
