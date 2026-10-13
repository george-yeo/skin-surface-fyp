#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace nacre::scene {

class Camera {
    public:
    Camera(glm::vec3 origin, glm::vec3 forward, glm::vec3 up, float nearPlane = 0.1f,
           float farPlane = 1000.0f) noexcept;

    virtual ~Camera() = default;

    glm::vec3 right() const noexcept { return glm::normalize(glm::cross(forward, up)); }

    glm::mat4 view() const noexcept { return glm::lookAt(origin, origin + forward, up); }

    virtual glm::mat4 projection() const noexcept = 0;

    // width / height of the viewport; called on resize instead of recreating
    // the camera, so position and orientation survive.
    virtual void setAspectRatio(float aspect) noexcept = 0;

    // nullopt if the ray is parallel to the plane
    std::optional<glm::vec3> unprojectToPlane(glm::vec2 ndc, float planeZ = 0.0f) const noexcept;

    // The point under `ndc` on the camera-facing plane `distance` units in
    // front of the camera. nullopt if the ray is parallel to that plane.
    std::optional<glm::vec3> unprojectToViewPlane(glm::vec2 ndc, float distance) const noexcept;

    const glm::vec3& getOrigin() const noexcept { return origin; }
    const glm::vec3& getForward() const noexcept { return forward; }
    const glm::vec3& getUp() const noexcept { return up; }
    const glm::quat& getRotation() const noexcept { return rotation; }

    void setOrigin(glm::vec3 o) noexcept { origin = o; }
    void setForward(glm::vec3 f) noexcept;
    void setUp(glm::vec3 u) noexcept;

    void translate(glm::vec3 translation) noexcept;
    void rotate(glm::vec3 rot) noexcept;

    // Back to the pose the camera was constructed with.
    void resetPose() noexcept;

    protected:
    // World-space ray through `ndc`, from the near plane towards the far plane:
    // {origin, direction} (direction not normalised).
    std::pair<glm::vec3, glm::vec3> screenRay(glm::vec2 ndc) const noexcept;

    static glm::mat4 vulkanCorrect(glm::mat4 proj) noexcept {
        proj[1][1] *= -1.0f;
        return proj;
    }

    float nearPlane;
    float farPlane;

    private:
    glm::vec3 origin;
    glm::quat rotation;
    glm::vec3 forward;
    glm::vec3 up;

    glm::vec3 initialOrigin;
    glm::vec3 initialForward;
    glm::vec3 initialUp;
};

class OrthographicCamera final : public Camera {
    public:
    OrthographicCamera(glm::vec3 origin, glm::vec3 forward, glm::vec3 up, float width, float height,
                       float nearPlane = 0.1f, float farPlane = 1000.0f) noexcept
        : Camera(origin, forward, up, nearPlane, farPlane), width(width), height(height) {}

    glm::mat4 projection() const noexcept override {
        glm::mat4 proj = glm::ortho(-width / 2.0f, width / 2.0f, -height / 2.0f, height / 2.0f,
                                    nearPlane, farPlane);
        return vulkanCorrect(proj);
    }

    float getWidth() const noexcept { return width; }
    float getHeight() const noexcept { return height; }
    void setWidth(float w) noexcept { width = w; }
    void setAspectRatio(float aspect) noexcept override { width = aspect * height; }
    void setHeight(float h) noexcept { height = h; }

    private:
    float width;
    float height;
};

class PerspectiveCamera final : public Camera {
    public:
    PerspectiveCamera(glm::vec3 origin, glm::vec3 forward, glm::vec3 up, float fov,
                      float aspectRatio, float nearPlane = 0.1f, float farPlane = 1000.0f) noexcept
        : Camera(origin, forward, up, nearPlane, farPlane), fov(fov), aspectRatio(aspectRatio) {}

    glm::mat4 projection() const noexcept override {
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        return vulkanCorrect(proj);
    }

    float getFov() const noexcept { return fov; }
    float getAspectRatio() const noexcept { return aspectRatio; }
    void setFov(float f) noexcept { fov = f; }
    void setAspectRatio(float a) noexcept override { aspectRatio = a; }

    private:
    float fov;
    float aspectRatio;
};

} // namespace nacre::scene
