#pragma once

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "camera.h"
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
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& viewPos,
                const std::array<Camera::FrustumPlane, 6>& frustumPlanes);

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

    // Culling methods
    bool IsBladeVisible(const glm::vec3& position, float height,
                        const std::array<Camera::FrustumPlane, 6>& planes)
    {
        // Test against all 6 frustum planes with blade height compensation
        for (const auto& plane : planes)
        {
            // Expand test point to account for blade height
            if (glm::dot(plane.normal, position) + plane.distance < -height * 0.5f)
            {
                return false;
            }
        }
        return true;
    }

    void CullGrassBlades(const std::array<Camera::FrustumPlane, 6>& planes)
    {
        visibleBlades.clear();
        visibleBlades.reserve(m_grassBlades.size()); // Avoid reallocations

        for (const auto& blade : m_grassBlades)
        {
            if (IsBladeVisible(blade.position, blade.height, planes))
            {
                visibleBlades.push_back(blade);
            }
        }
    }

    std::vector<GrassBlade> visibleBlades; // New container for culled blades
};