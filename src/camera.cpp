#include "camera.h"
#include <iostream>

Camera::Camera(glm::vec3 target, float distance, float height)
    : target(target)
    , distance(distance)
    , height(height)
    , yaw(-90.0f)
    , pitch(0.0f)
    , worldUp(glm::vec3(0.0f, 1.0f, 0.0f))
    , movementSpeed(5.0f)
    , mouseSensitivity(0.1f)
    , zoomSensitivity(1.0f)
    , minDistance(8.0f)
    , maxDistance(15.0f)
{
    updateCameraVectors();
}

std::array<FrustumPlane, 6> Camera::getFrustumPlanes() const
{
    std::array<FrustumPlane, 6> planes;
    const glm::mat4 vp = getProjectionMatrix() * getViewMatrix();

    // Left plane
    planes[0] = { .normal =
                      glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]),
                  .distance = vp[3][3] + vp[3][0] };
    // Right plane (repeat pattern for other planes)
    planes[1] = { /* ... */ };
    // Bottom, Top, Near, Far planes...

    // Normalize all planes
    for (auto& plane : planes)
    {
        float length = glm::length(plane.normal);
        plane.normal /= length;
        plane.distance /= length;
    }
    return planes;
}

void Camera::processMouseMovement(float xoffset, float yoffset)
{
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Constrain pitch to prevent flipping
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset)
{
    distance -= yoffset * zoomSensitivity;
    if (distance < minDistance)
        distance = minDistance;
    if (distance > maxDistance)
        distance = maxDistance;
    updateCameraVectors();
}

void Camera::updatePosition(glm::vec3 newTarget)
{
    target = newTarget;
    updateCameraVectors();
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));

    // // Calculate horizontal and vertical offsets
    // float horizontalDistance = distance * cos(glm::radians(pitch));
    // float verticalOffset = distance * sin(glm::radians(pitch));

    // // Calculate new position
    // position.x = target.x - horizontalDistance * cos(glm::radians(yaw));
    // position.y = target.y + height + verticalOffset;
    // position.z = target.z - horizontalDistance * sin(glm::radians(yaw));

    // // Recalculate front vector (points toward target)
    // front = glm::normalize(target - position);
    // right = glm::normalize(glm::cross(front, worldUp));
    // up = glm::normalize(glm::cross(right, front));
}

std::array<Camera::FrustumPlane, 6> Camera::GetFrustumPlanes(float aspectRatio) const
{
    std::array<FrustumPlane, 6> planes;
    const glm::mat4 vp = GetProjectionMatrix(aspectRatio) * GetViewMatrix();

    // Left plane
    planes[0] = { glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]),
                  vp[3][3] + vp[3][0] };

    // Right plane
    planes[1] = { glm::vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]),
                  vp[3][3] - vp[3][0] };

    // Bottom plane
    planes[2] = { glm::vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]),
                  vp[3][3] + vp[3][1] };

    // Top plane
    planes[3] = { glm::vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]),
                  vp[3][3] - vp[3][1] };

    // Near plane
    planes[4] = { glm::vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]),
                  vp[3][3] + vp[3][2] };

    // Far plane
    planes[5] = { glm::vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]),
                  vp[3][3] - vp[3][2] };

    // Normalize all planes
    for (auto& plane : planes)
    {
        float length = glm::length(plane.normal);
        plane.normal /= length;
        plane.distance /= length;
    }

    return planes;
}

glm::mat4 Camera::getViewMatrix() const { return glm::lookAt(position, target, up); }

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float near, float far) const
{
    return glm::perspective(glm::radians(Zoom), aspectRatio, near, far);
}

glm::vec3 Camera::getPosition() const { return position; }