#pragma once

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "shader.h"

struct GrassBlade
{
    glm::vec3 position;
    float width;
    float height;
    glm::vec3 color;
    float rotation;
};

class GrassManager
{
public:
    GrassManager();
    ~GrassManager();

    void initialize(int numBlades, float areaWidth, float areaDepth);
    void update(float deltaTime, const glm::vec3& windDirection);
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos);

    void setWindStrength(float strength) { m_windStrength = strength; }

private:
    void generateGrassBlades(int numBlades, float areaWidth, float areaDepth);
    void setupBuffers();

    std::vector<GrassBlade> m_grassBlades;
    Shader m_grassShader;

    GLuint m_VAO;
    GLuint m_VBO;
    GLuint m_instanceVBO;

    float m_windStrength;
    float m_time;
    glm::vec3 m_windDirection;

    const std::vector<float> m_bladeVertices = { -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                                                 0.5f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                                 0.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f };
};