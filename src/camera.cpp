#include "camera.h"
#include <array>
#include <iostream>

Camera::Camera(glm::vec3 target, float distance, float height)
    : target(target)
    , distance(distance)
    , height(height)
    , yaw(-90.0f)
    , pitch(0.0f)
    , zoom(60.f)
    , worldUp(glm::vec3(0.0f, 1.0f, 0.0f))
    , movementSpeed(5.0f)
    , mouseSensitivity(0.1f)
    , zoomSensitivity(1.0f)
    , minDistance(8.0f)
    , maxDistance(15.0f)
{
    updateCameraVectors();
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
    float horizontalDistance = distance * cos(glm::radians(pitch));
    float verticalOffset = distance * sin(glm::radians(pitch));

    position.x = target.x - horizontalDistance * cos(glm::radians(yaw));
    position.y = target.y + height + verticalOffset;
    position.z = target.z - horizontalDistance * sin(glm::radians(yaw));

    front = glm::normalize(target - position);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));

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

std::array<Camera::FrustumPlane, 6> Camera::getFrustumPlanes(float aspectRatio) const
{
    std::array<FrustumPlane, 6> planes;
    const glm::mat4 vp = getProjectionMatrix(aspectRatio) * getViewMatrix();

    // Each plane: Ax + By + Cz + D = 0, extracted from rows of the matrix

    // Left
    planes[0].normal.x = vp[0][3] + vp[0][0];
    planes[0].normal.y = vp[1][3] + vp[1][0];
    planes[0].normal.z = vp[2][3] + vp[2][0];
    planes[0].distance = vp[3][3] + vp[3][0];

    // Right
    planes[1].normal.x = vp[0][3] - vp[0][0];
    planes[1].normal.y = vp[1][3] - vp[1][0];
    planes[1].normal.z = vp[2][3] - vp[2][0];
    planes[1].distance = vp[3][3] - vp[3][0];

    // Bottom
    planes[2].normal.x = vp[0][3] + vp[0][1];
    planes[2].normal.y = vp[1][3] + vp[1][1];
    planes[2].normal.z = vp[2][3] + vp[2][1];
    planes[2].distance = vp[3][3] + vp[3][1];

    // Top
    planes[3].normal.x = vp[0][3] - vp[0][1];
    planes[3].normal.y = vp[1][3] - vp[1][1];
    planes[3].normal.z = vp[2][3] - vp[2][1];
    planes[3].distance = vp[3][3] - vp[3][1];

    // Near
    planes[4].normal.x = vp[0][3] + vp[0][2];
    planes[4].normal.y = vp[1][3] + vp[1][2];
    planes[4].normal.z = vp[2][3] + vp[2][2];
    planes[4].distance = vp[3][3] + vp[3][2];

    // Far
    planes[5].normal.x = vp[0][3] - vp[0][2];
    planes[5].normal.y = vp[1][3] - vp[1][2];
    planes[5].normal.z = vp[2][3] - vp[2][2];
    planes[5].distance = vp[3][3] - vp[3][2];
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
    return glm::perspective(glm::radians(zoom), aspectRatio, near, far);
}

glm::vec3 Camera::getPosition() const { return position; }