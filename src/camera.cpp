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
    // Calculate horizontal and vertical offsets
    float horizontalDistance = distance * cos(glm::radians(pitch));
    float verticalOffset = distance * sin(glm::radians(pitch));

    // Calculate new position
    position.x = target.x - horizontalDistance * cos(glm::radians(yaw));
    position.y = target.y + height + verticalOffset;
    position.z = target.z - horizontalDistance * sin(glm::radians(yaw));

    // Recalculate front vector (points toward target)
    front = glm::normalize(target - position);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getViewMatrix() const { return glm::lookAt(position, target, up); }

glm::vec3 Camera::getPosition() const { return position; }