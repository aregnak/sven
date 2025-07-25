#pragma once

#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <array>
#include <vector>

class Camera
{
public:
    struct FrustumPlane
    {
        glm::vec3 normal;
        float distance;
    };

    enum class Movement
    {
        FOREWARD,
        BACKWARD,
        LEFT,
        RIGHT
    };

    Camera(glm::vec3 target = glm::vec3(0.0f), float distance = 10.f, float height = 2.f);

    void processMouseMovement(float xoffset, float yoffset);
    void processMouseScroll(float yoffset);
    void updatePosition(glm::vec3 newTarget);

    std::array<FrustumPlane, 6> getFrustumPlanes(float aspectRatio) const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio, float near = 0.1f, float far = 100.0f) const;
    glm::vec3 getPosition() const;
    float getYaw() const { return yaw; }

    // In case
    float getDistance() const { return distance; }
    float getHeight() const { return height; }

private:
    void updateCameraVectors();

    glm::vec3 target;
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float distance;
    float height;
    float zoom;

    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoomSensitivity;
    float minDistance;
    float maxDistance;
};