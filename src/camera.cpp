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
    pitch -= yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    updateCameraVectors();
}

void Camera::updatePosition(glm::vec3 newTarget)
{
    target = newTarget;
    updateCameraVectors();
}

void Camera::processKeyboard(Camera::Movement direction, float deltaTime)
{
    float velocity = movementSpeed * deltaTime;
    if (direction == Camera::Movement::FORWARD)
        position += front * velocity;
    if (direction == Camera::Movement::BACKWARD)
        position -= front * velocity;
    if (direction == Camera::Movement::LEFT)
        position -= right * velocity;
    if (direction == Camera::Movement::RIGHT)
        position += right * velocity;
    if (direction == Camera::Movement::UP)
        position += up * velocity;
    if (direction == Camera::Movement::DOWN)
        position -= up * velocity;
}

void Camera::updateCameraVectors()
{
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    // Also re-calculate the right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getViewMatrix() const { return glm::lookAt(position, position + front, up); }

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float near, float far) const
{
    return glm::perspective(glm::radians(zoom), aspectRatio, near, far);
}

glm::vec3 Camera::getPosition() const { return position; }