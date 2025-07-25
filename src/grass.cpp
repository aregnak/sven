#include "grass.h"
#include <random>
#include <iostream>

GrassManager::GrassManager()
    : m_windStrength(0.5f)
    , m_time(0.0f)
    , m_windDirection(1.0f, 0.0f, 0.0f)
    , m_grassShader("shaders/grass.vert.glsl", "shaders/grass.frag.glsl")
{
}

GrassManager::~GrassManager()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_instanceVBO);
}

void GrassManager::initialize(int numBlades, float areaWidth, float areaDepth)
{
    generateGrassBlades(numBlades, areaWidth, areaDepth);
    setupBuffers();
}

void GrassManager::generateGrassBlades(int numBlades, float areaWidth, float areaDepth)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posXDist(-areaWidth / 2, areaWidth / 2);
    std::uniform_real_distribution<float> posZDist(-areaDepth / 2, areaDepth / 2);
    std::uniform_real_distribution<float> heightDist(0.3f, 0.7f);
    std::uniform_real_distribution<float> widthDist(0.02f, 0.05f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> colorDist(0.7f, 1.0f);

    m_grassBlades.resize(numBlades);
    for (int i = 0; i < numBlades; ++i)
    {
        m_grassBlades[i].position = glm::vec3(posXDist(gen), 0.0f, posZDist(gen));
        m_grassBlades[i].height = heightDist(gen);
        m_grassBlades[i].width = widthDist(gen);
        m_grassBlades[i].rotation = rotDist(gen);
        m_grassBlades[i].color =
            glm::vec3(0.1f * colorDist(gen), 0.6f * colorDist(gen), 0.1f * colorDist(gen));
    }
}

void GrassManager::setupBuffers()
{
    // Setup VAO/VBO for grass blade geometry
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_bladeVertices.size() * sizeof(float), &m_bladeVertices[0],
                 GL_STATIC_DRAW);

    // Vertex attributes
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // TexCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    // Setup instance VBO
    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, m_grassBlades.size() * sizeof(GrassBlade), nullptr,
                 GL_DYNAMIC_DRAW);
    // Instance attributes
    // Position
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(GrassBlade), (void*)0);
    glVertexAttribDivisor(3, 1);
    // Width
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GrassBlade),
                          (void*)offsetof(GrassBlade, width));
    glVertexAttribDivisor(4, 1);
    // Height
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(GrassBlade),
                          (void*)offsetof(GrassBlade, height));
    glVertexAttribDivisor(5, 1);
    // Color
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(GrassBlade),
                          (void*)offsetof(GrassBlade, color));
    glVertexAttribDivisor(6, 1);
    // Rotation
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(GrassBlade),
                          (void*)offsetof(GrassBlade, rotation));
    glVertexAttribDivisor(7, 1);

    glBindVertexArray(0);
}

void GrassManager::update(float deltaTime, const glm::vec3& windDirection)
{
    m_time += deltaTime;
    m_windDirection = windDirection;
}

void GrassManager::render(const glm::mat4& view, const glm::mat4& projection,
                          const glm::vec3& viewPos,
                          const std::array<Camera::FrustumPlane, 6>& frustumPlanes)
{
    // Only update if view has changed
    static glm::mat4 lastView;
    static glm::vec3 lastPos;
    glm::vec3 currentPos = glm::vec3(view[3]);

    // Check position change magnitude (more reliable than matrix comparison)
    if (glm::length(currentPos - lastPos) > 0.1f)
    {
        CullGrassBlades(frustumPlanes);
        lastView = view;
        lastPos = currentPos;

        // Only upload when culling changes
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, visibleBlades.size() * sizeof(GrassBlade),
                        visibleBlades.data());
    }

    // Render instances
    m_grassShader.use();
    m_grassShader.setMat4("view", view);
    m_grassShader.setMat4("projection", projection);
    m_grassShader.setVec3("viewPos", viewPos);
    m_grassShader.setFloat("time", m_time);
    m_grassShader.setVec3("windDirection", m_windDirection);
    m_grassShader.setFloat("windStrength", m_windStrength);

    glBindVertexArray(m_VAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, visibleBlades.size());
}