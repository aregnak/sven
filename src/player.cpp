#include "player.h"
#include <iostream>

Player::Player(glm::vec3 pos)
    : position(pos)
    , velocity(0.0f)
    , speed(5.f)
    , gravity(-9.8f)
    , jumpStrength(5.f)
    , isGrounded(false)
{
}

void Player::processInput(float deltaTime, bool forward, bool backward, bool left, bool right,
                          bool jump, float cameraYaw)
{
    glm::vec3 direction(0.0f);

    // Convert camera yaw to radians
    float yawRad = glm::radians(cameraYaw);

    // Calculate forward and right vectors based on camera orientation
    // Align with camera calculation in main.cpp: camPos.x = playerPos.x - horizontalDistance * cos(yaw)
    // The camera's forward direction is the direction from camera to player (behind to front)
    glm::vec3 forwardDir =
        glm::vec3(cos(yawRad), 0.0f, sin(yawRad)); // Same as camera offset direction
    // Camera right direction (perpendicular to forward)
    glm::vec3 rightDir = glm::vec3(-sin(yawRad), 0.0f, cos(yawRad));

    if (forward)
        direction += forwardDir;
    if (backward)
        direction -= forwardDir;
    if (left)
        direction -= rightDir;
    if (right)
        direction += rightDir;

    if (glm::length(direction) > 0.0f)
        move(glm::normalize(direction), deltaTime);

    if (jump && isGrounded)
    {
        velocity.y = jumpStrength;
        isGrounded = false;
    }
}

void Player::move(const glm::vec3& dir, float deltaTime) { position += dir * speed * deltaTime; }

void Player::applyGravity(float deltaTime)
{
    if (!isGrounded)
    {
        velocity.y += gravity * deltaTime;
        position.y += velocity.y * deltaTime;

        if (position.y <= 0.0f)
        { // simple ground check
            position.y = 0.0f;
            isGrounded = true;
            velocity.y = 0.0f;
        }
    }
}

void Player::update(float deltaTime) { applyGravity(deltaTime); }

glm::vec3 Player::getPosition() const { return position; }

glm::vec3 Player::getVelocity() const { return velocity; }

void Player::setVelocity(const glm::vec3& vel) { velocity = vel; }
