#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Player
{
public:
    Player(glm::vec3 position);

    void processInput(float deltaTime, bool moveForward, bool moveBackward, bool moveLeft,
                      bool moveRight, bool jump);
    void applyGravity(float deltaTime);
    void update(float deltaTime);

    glm::vec3 getPosition() const;
    glm::vec3 getVelocity() const;
    void setVelocity(const glm::vec3& velocity);

private:
    glm::vec3 position;
    glm::vec3 velocity;
    float speed;
    float gravity;
    float jumpStrength;
    bool isGrounded;

    void move(const glm::vec3& direction, float deltaTime);
};