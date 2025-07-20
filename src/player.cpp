#include "player.h"

Player::Player(glm::vec3 pos)
    : position(pos)
    , velocity(0.0f)
{
}

void Player::processInput(float deltaTime, bool forward, bool backward, bool left, bool right,
                          bool jump)
{
    glm::vec3 direction(0.0f);

    if (forward)
        direction.z -= 1.0f;
    if (backward)
        direction.z += 1.0f;
    if (left)
        direction.x -= 1.0f;
    if (right)
        direction.x += 1.0f;

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
