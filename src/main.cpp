#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <map>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <ctime>
#include "shader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// FastNoiseLite for procedural generation
#define FASTNOISE_LITE_IMPLEMENTATION
#include "FastNoiseLite.h"

#include "player.h"

// Forward declarations
class GrassManager;
class GrassManagerGeom;

// Global variables
const int SCR_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// Camera variables
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float cameraDistance = 10.0f;
float cameraHeight = 2.0f;

// Grass system
GrassManager* grassManager = nullptr;
GrassManagerGeom* grassManagerGeom = nullptr;

// Terrain generation functions
struct TerrainVertex
{
    glm::vec3 position;
    glm::vec2 texCoord;
};

// Grass blade structure
struct GrassBlade
{
    glm::vec3 position;
    float height;
    float width;
    float rotation;
    float bend;
};

// Grass vertex structure
struct GrassVertex
{
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
};

// Grass Manager Class (similar to the reference)
class GrassManager
{
private:
    std::vector<glm::mat4> grassMatrices;
    std::vector<GrassVertex> grassVertices;
    std::vector<unsigned int> grassIndices;

    GLuint grassVAO, grassVBO, grassEBO;
    GLuint grassMatrixVBO; // For instanced matrix data
    GLuint grassTexture;
    Shader* grassShader;

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    glm::vec3 cameraPosition;

    // Terrain reference for height sampling
    std::vector<TerrainVertex>* terrainVertices;
    int terrainWidth, terrainHeight;
    float terrainScale;

    // Instanced rendering parameters
    const int nbGrassElemSide = 300; // Larger grid to cover whole terrain
    const float sizeToDraw = 100.0f; // Match actual terrain size (1000x1000 units)
    uint nbGrassToDraw;

public:
    GrassManager()
        : grassShader(nullptr)
        , terrainVertices(nullptr)
        , nbGrassToDraw(0)
    {
    }

    void init(std::vector<TerrainVertex>* terrain, int width, int height, float scale)
    {
        terrainVertices = terrain;
        terrainWidth = width;
        terrainHeight = height;
        terrainScale = scale;

        // Create shader for grass
        grassShader = new Shader("shaders/grass_vertex.glsl", "shaders/fragment.glsl");

        generateGrassMatrices();
        createGrassGeometry();
        createGrassTexture();
        setupInstancedBuffers();

        std::cout << "GrassManager initialized with " << nbGrassToDraw << " blades" << std::endl;
    }

    void setViewMatrix(const glm::mat4& view) { viewMatrix = view; }
    void setProjectionMatrix(const glm::mat4& proj) { projectionMatrix = proj; }
    void setCameraPosition(const glm::vec3& pos) { cameraPosition = pos; }

    void draw()
    {
        if (!grassShader)
            return;

        grassShader->use();

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        grassShader->setInt("texture1", 0);

        // Set matrices
        grassShader->setMat4("view", viewMatrix);
        grassShader->setMat4("projection", projectionMatrix);

        // Enable alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);

        glBindVertexArray(grassVAO);

        // Draw all grass blades (efficient smaller area)
        glDrawElementsInstanced(GL_TRIANGLES, grassIndices.size(), GL_UNSIGNED_INT, 0,
                                nbGrassToDraw);

        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }

    void cleanup()
    {
        glDeleteVertexArrays(1, &grassVAO);
        glDeleteBuffers(1, &grassVBO);
        glDeleteBuffers(1, &grassEBO);
        glDeleteBuffers(1, &grassMatrixVBO);
        glDeleteTextures(1, &grassTexture);

        if (grassShader)
        {
            delete grassShader;
            grassShader = nullptr;
        }
    }

private:
    float getTerrainHeight(float worldX, float worldZ)
    {
        if (!terrainVertices)
            return 0.0f;

        // Account for terrain offset (terrain is translated by -50, 0, -50)
        float adjustedX = worldX + 50.0f;
        float adjustedZ = worldZ + 50.0f;

        // Convert to grid coordinates
        float gridX = adjustedX / terrainScale;
        float gridZ = adjustedZ / terrainScale;

        int x0 = (int)gridX;
        int z0 = (int)gridZ;
        int x1 = x0 + 1;
        int z1 = z0 + 1;

        // Clamp to terrain bounds
        x0 = std::max(0, std::min(terrainWidth - 1, x0));
        x1 = std::max(0, std::min(terrainWidth - 1, x1));
        z0 = std::max(0, std::min(terrainHeight - 1, z0));
        z1 = std::max(0, std::min(terrainHeight - 1, z1));

        // Get the four corner heights
        float h00 = (*terrainVertices)[z0 * terrainWidth + x0].position.y;
        float h10 = (*terrainVertices)[z0 * terrainWidth + x1].position.y;
        float h01 = (*terrainVertices)[z1 * terrainWidth + x0].position.y;
        float h11 = (*terrainVertices)[z1 * terrainWidth + x1].position.y;

        // Calculate interpolation weights
        float fx = gridX - x0;
        float fz = gridZ - z0;

        // Bilinear interpolation
        float h0 = h00 * (1 - fx) + h10 * fx;
        float h1 = h01 * (1 - fx) + h11 * fx;
        return h0 * (1 - fz) + h1 * fz;
    }

    void generateGrassMatrices()
    {
        grassMatrices.clear();

        // Initialize random seed
        srand(time(0));

        // Generate grass matrices in a grid pattern like the reference
        for (int i = 0; i < nbGrassElemSide; i++)
        {
            for (int j = 0; j < nbGrassElemSide; j++)
            {
                // Calculate position in world space (similar to reference)
                float posX = -(sizeToDraw / 2.0f) * (1 - (float(i) / (nbGrassElemSide - 1))) +
                             (sizeToDraw / 2.0f) * (float(i) / (nbGrassElemSide - 1));
                float posZ = -(sizeToDraw / 2.0f) * (1 - (float(j) / (nbGrassElemSide - 1))) +
                             (sizeToDraw / 2.0f) * (float(j) / (nbGrassElemSide - 1));

                // Add small random offset
                int valRand = rand() % 1000;
                float fRand = float(valRand) / 1000.0f;
                posX += fRand / 10.0f - 0.05f;
                posZ += fRand / 10.0f - 0.05f;

                // Get terrain height at this position
                float terrainHeight = getTerrainHeight(posX, posZ);

                // Skip if terrain is too low or steep
                if (terrainHeight < -10.0f)
                    continue;

                float slope = calculateSlope(posX, posZ);
                if (slope > 1.5f)
                    continue;

                // Create transformation matrix
                glm::mat4 modelMatrix = glm::mat4(1.0f);

                // Position on terrain (grass blade base should be at terrain height)
                modelMatrix = glm::translate(modelMatrix, glm::vec3(posX, terrainHeight, posZ));

                // Random rotation around Y axis
                modelMatrix =
                    glm::rotate(modelMatrix, posX * posZ * 5.244f, glm::vec3(0.0f, 1.0f, 0.0f));

                // Scale (similar to reference) ter
                modelMatrix = glm::scale(modelMatrix, glm::vec3(0.3f, 1.0f, 0.3f));
                modelMatrix = glm::scale(modelMatrix, glm::vec3(1.0f, 1.0f + fRand, 1.0f));

                grassMatrices.push_back(modelMatrix);
            }
        }

        nbGrassToDraw = grassMatrices.size();
    }

    float calculateSlope(float worldX, float worldZ)
    {
        // Calculate slope by sampling nearby points
        float delta = terrainScale * 0.5f;

        float height = getTerrainHeight(worldX, worldZ);
        float left = getTerrainHeight(worldX - delta, worldZ);
        float right = getTerrainHeight(worldX + delta, worldZ);
        float up = getTerrainHeight(worldX, worldZ - delta);
        float down = getTerrainHeight(worldX, worldZ + delta);

        float dx = (right - left) / (2.0f * delta);
        float dz = (down - up) / (2.0f * delta);

        return sqrt(dx * dx + dz * dz);
    }

    void createGrassGeometry()
    {
        grassVertices.clear();
        grassIndices.clear();

        // Create grass blade quad
        grassVertices.push_back(
            { glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
        grassVertices.push_back(
            { glm::vec3(0.5f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
        grassVertices.push_back(
            { glm::vec3(-0.5f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
        grassVertices.push_back(
            { glm::vec3(0.5f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f) });

        grassIndices = { 0, 1, 2, 1, 3 };
    }

    void createGrassTexture()
    {
        const int texSize = 64;
        std::vector<unsigned char> textureData(texSize * texSize * 4);

        for (int y = 0; y < texSize; ++y)
        {
            for (int x = 0; x < texSize; ++x)
            {
                int index = (y * texSize + x) * 4;
                float gradient = (float)y / texSize;
                float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.2f;
                gradient += noise;
                gradient = std::max(0.0f, std::min(1.0f, gradient));

                unsigned char green = 50 + (unsigned char)(gradient * 150);
                unsigned char red = 20 + (unsigned char)(gradient * 40);
                unsigned char blue = 10 + (unsigned char)(gradient * 20);

                unsigned char alpha = 255;
                if (x < 5 || x > texSize - 5)
                    alpha =
                        (unsigned char)(255 * (1.0f - abs(x - texSize / 2.0f) / (texSize / 2.0f)));

                textureData[index] = red;
                textureData[index + 1] = green;
                textureData[index + 2] = blue;
                textureData[index + 3] = alpha;
            }
        }

        glGenTextures(1, &grassTexture);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     textureData.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    void setupInstancedBuffers()
    {
        glGenVertexArrays(1, &grassVAO);
        glGenBuffers(1, &grassVBO);
        glGenBuffers(1, &grassEBO);
        glGenBuffers(1, &grassMatrixVBO);

        glBindVertexArray(grassVAO);

        // Setup vertex data
        glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
        glBufferData(GL_ARRAY_BUFFER, grassVertices.size() * sizeof(GrassVertex),
                     grassVertices.data(), GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GrassVertex), (void*)0);
        glEnableVertexAttribArray(0);

        // Texture coordinate attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
                              (void*)offsetof(GrassVertex, texCoord));
        glEnableVertexAttribArray(1);

        // Normal attribute
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
                              (void*)offsetof(GrassVertex, normal));
        glEnableVertexAttribArray(2);

        // Setup indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grassEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, grassIndices.size() * sizeof(unsigned int),
                     grassIndices.data(), GL_STATIC_DRAW);

        // Setup instanced matrix data
        glBindBuffer(GL_ARRAY_BUFFER, grassMatrixVBO);
        glBufferData(GL_ARRAY_BUFFER, grassMatrices.size() * sizeof(glm::mat4),
                     grassMatrices.data(), GL_STATIC_DRAW);

        // Set up matrix attributes for instancing (4 vec4s per matrix)
        uint32_t vec4Size = sizeof(glm::vec4);

        // Matrix column 0
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)0);
        glVertexAttribDivisor(3, 1);

        // Matrix column 1
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(uintptr_t)(vec4Size));
        glVertexAttribDivisor(4, 1);

        // Matrix column 2
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size,
                              (void*)(uintptr_t)(2 * vec4Size));
        glVertexAttribDivisor(5, 1);

        // Matrix column 3
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size,
                              (void*)(uintptr_t)(3 * vec4Size));
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }
};

// Geometry shader version (placeholder for future implementation)
class GrassManagerGeom
{
public:
    void init(std::vector<TerrainVertex>* terrain, int width, int height, float scale)
    {
        // TODO: Implement geometry shader version
        std::cout << "GrassManagerGeom initialized (placeholder)" << std::endl;
    }

    void setViewMatrix(const glm::mat4& view) {}
    void setProjectionMatrix(const glm::mat4& proj) {}
    void setCameraPosition(const glm::vec3& pos) {}

    void draw() {}
    void cleanup() {}
};

// Helper function to extract transformation from a glTF node
glm::mat4 getNodeTransform(const tinygltf::Node& node)
{
    glm::mat4 transform = glm::mat4(1.0f);

    // Apply translation
    if (node.translation.size() >= 3)
    {
        transform = glm::translate(
            transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
    }

    // Apply rotation (quaternion)
    if (node.rotation.size() >= 4)
    {
        glm::quat rot(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        transform = transform * glm::mat4_cast(rot);
    }

    // Apply scale
    if (node.scale.size() >= 3)
    {
        transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    }

    // Apply matrix (if present, overrides the above)
    if (node.matrix.size() >= 16)
    {
        glm::mat4 matrix;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                matrix[i][j] = node.matrix[i * 4 + j];
            }
        }
        transform = matrix;
    }

    return transform;
}

// Helper function to traverse scene hierarchy and build mesh transformations
void traverseScene(const tinygltf::Model& model, int nodeIndex,
                   const std::vector<glm::mat4>& nodeTransforms, glm::mat4 parentTransform,
                   std::map<int, glm::mat4>& meshTransforms)
{
    if (nodeIndex < 0 || nodeIndex >= model.nodes.size())
    {
        return;
    }

    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 localTransform = nodeTransforms[nodeIndex];
    glm::mat4 worldTransform = parentTransform * localTransform;

    // If this node has a mesh, store its world transformation
    if (node.mesh >= 0)
    {
        meshTransforms[node.mesh] = worldTransform;
        std::cout << "Found mesh " << node.mesh << " at node " << nodeIndex << std::endl;
    }

    // Traverse children
    for (int childIndex : node.children)
    {
        traverseScene(model, childIndex, nodeTransforms, worldTransform, meshTransforms);
    }
}

// Helper function to build complete transformation matrix for a node (including parents)
glm::mat4 buildNodeTransform(const tinygltf::Model& model, int nodeIndex,
                             const std::vector<glm::mat4>& nodeTransforms)
{
    if (nodeIndex < 0 || nodeIndex >= model.nodes.size())
    {
        return glm::mat4(1.0f);
    }

    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 localTransform = nodeTransforms[nodeIndex];

    // Find parent node
    int parentIndex = -1;
    for (size_t i = 0; i < model.nodes.size(); ++i)
    {
        const auto& potentialParent = model.nodes[i];
        for (int child : potentialParent.children)
        {
            if (child == nodeIndex)
            {
                parentIndex = i;
                break;
            }
        }
        if (parentIndex != -1)
            break;
    }

    // If no parent, return local transform
    if (parentIndex == -1)
    {
        return localTransform;
    }

    // Recursively get parent transform and combine
    glm::mat4 parentTransform = buildNodeTransform(model, parentIndex, nodeTransforms);
    return parentTransform * localTransform;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY;

    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    cameraDistance -= (float)yoffset;
    if (cameraDistance < 8.0f)
        cameraDistance = 8.0f;
    if (cameraDistance > 15.0f)
        cameraDistance = 15.0f;
}

std::vector<TerrainVertex> generateTerrainVertices(int width, int height, float scale)
{
    std::vector<TerrainVertex> vertices;

    // Create multiple noise layers for more interesting terrain
    FastNoiseLite baseNoise;
    baseNoise.SetSeed(1337);
    baseNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    baseNoise.SetFrequency(0.02f);
    baseNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    baseNoise.SetFractalOctaves(5);
    baseNoise.SetFractalLacunarity(2.0f);
    baseNoise.SetFractalGain(0.5f);

    // Create detail noise for small features
    FastNoiseLite detailNoise;
    detailNoise.SetSeed(42);
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    detailNoise.SetFrequency(0.1f);
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(3);
    detailNoise.SetFractalLacunarity(2.0f);
    detailNoise.SetFractalGain(0.5f);

    for (int z = 0; z < height; ++z)
    {
        for (int x = 0; x < width; ++x)
        {
            // Generate height using multiple noise layers
            float worldX = x * scale;
            float worldZ = z * scale;

            // Base terrain
            float baseHeight = baseNoise.GetNoise(worldX, worldZ);

            // Detail terrain
            float detailHeight = detailNoise.GetNoise(worldX, worldZ) * 0.3f;

            // Combine noise layers
            float heightValue = (baseHeight + detailHeight) * 15.0f;

            // Add some valleys and peaks
            float distanceFromCenter = sqrt(worldX * worldX + worldZ * worldZ);
            float centerFalloff = 1.0f - (distanceFromCenter / 100.0f);
            if (centerFalloff > 0)
            {
                heightValue += centerFalloff * 5.0f;
            }

            // Create vertex
            TerrainVertex vertex;
            vertex.position = glm::vec3(worldX, heightValue, worldZ);
            vertex.texCoord = glm::vec2((float)x / (width - 1), (float)z / (height - 1));
            vertices.push_back(vertex);
        }
    }

    return vertices;
}

std::vector<unsigned int> generateTerrainIndices(int width, int height)
{
    std::vector<unsigned int> indices;

    for (int z = 0; z < height - 1; ++z)
    {
        for (int x = 0; x < width - 1; ++x)
        {
            unsigned int topLeft = z * width + x;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (z + 1) * width + x;
            unsigned int bottomRight = bottomLeft + 1;

            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    return indices;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "sven", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    // VSYNC
    // glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl");

    // Load GLTF model
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Player model
    Player player(glm::vec3(0.0f, 15.0f, 15.0f)); // Start above terrain

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, "Assets/Characters/gltf/Knight.glb");

    if (!warn.empty())
        std::cout << "Warn: " << warn << std::endl;
    if (!err.empty())
        std::cerr << "Err: " << err << std::endl;

    if (!ret)
    {
        std::cerr << "Failed to load glTF" << std::endl;
        return -1;
    }

    // Load all meshes from the glTF file
    std::vector<GLuint> vaos;
    std::vector<GLuint> vbos;
    std::vector<GLuint> ebos;
    std::vector<size_t> indexCounts;
    std::vector<GLenum> indexTypes;
    std::vector<glm::mat4> localTransforms; // Store local transformations

    // First, build local transformations for all nodes
    std::vector<glm::mat4> nodeTransforms;
    for (const auto& node : model.nodes)
    {
        nodeTransforms.push_back(getNodeTransform(node));
    }

    // Build complete transformations for each mesh
    std::map<int, glm::mat4> meshTransforms;

    // Traverse the scene hierarchy starting from scene root nodes
    if (!model.scenes.empty())
    {
        const auto& defaultScene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
        for (int rootNodeIndex : defaultScene.nodes)
        {
            traverseScene(model, rootNodeIndex, nodeTransforms, glm::mat4(1.0f), meshTransforms);
        }
    }
    else
    {
        // Fallback: traverse all nodes that don't have parents
        for (size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
        {
            const auto& node = model.nodes[nodeIndex];
            if (node.mesh >= 0)
            { // This node has a mesh
                glm::mat4 completeTransform = buildNodeTransform(model, nodeIndex, nodeTransforms);
                meshTransforms[node.mesh] = completeTransform;
                std::cout << "Mesh " << node.mesh << " transform: " << std::endl;
                for (int i = 0; i < 4; ++i)
                {
                    for (int j = 0; j < 4; ++j)
                    {
                        std::cout << completeTransform[i][j] << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }
    }

    std::cout << "Total meshes: " << model.meshes.size() << std::endl;
    std::cout << "Total nodes: " << model.nodes.size() << std::endl;
    std::cout << "Total scenes: " << model.scenes.size() << std::endl;

    if (!model.scenes.empty())
    {
        const auto& defaultScene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
        std::cout << "Default scene nodes: " << defaultScene.nodes.size() << std::endl;
        for (int nodeIndex : defaultScene.nodes)
        {
            std::cout << "Scene node: " << nodeIndex << std::endl;
        }
    }

    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        const auto& mesh = model.meshes[meshIndex];

        // Get the transformation for this mesh
        glm::mat4 meshTransform = glm::mat4(1.0f);
        auto transformIt = meshTransforms.find(meshIndex);
        if (transformIt != meshTransforms.end())
        {
            meshTransform = transformIt->second;
        }

        for (const auto& primitive : mesh.primitives)
        {
            // Check if required attributes exist
            auto posIt = primitive.attributes.find("POSITION");
            auto texIt = primitive.attributes.find("TEXCOORD_0");
            if (posIt == primitive.attributes.end() || texIt == primitive.attributes.end())
            {
                std::cerr << "Skipping primitive with missing attributes" << std::endl;
                continue;
            }

            // Store the mesh transformation
            localTransforms.push_back(meshTransform);

            // Positions
            const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(
                &(posBuffer.data[posView.byteOffset + posAccessor.byteOffset]));

            // Indices
            const tinygltf::Accessor& idxAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& idxView = model.bufferViews[idxAccessor.bufferView];
            const tinygltf::Buffer& idxBuffer = model.buffers[idxView.buffer];
            const void* indices = &(idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset]);

            // Texture coordinates
            const tinygltf::Accessor& texAccessor = model.accessors[texIt->second];
            const tinygltf::BufferView& texView = model.bufferViews[texAccessor.bufferView];
            const tinygltf::Buffer& texBuffer = model.buffers[texView.buffer];
            const float* texcoords = reinterpret_cast<const float*>(
                &(texBuffer.data[texView.byteOffset + texAccessor.byteOffset]));

            // Create OpenGL buffers for this primitive
            GLuint vao, vbo, ebo;
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            glBindVertexArray(vao);

            // Interleave vertex data
            std::vector<float> vertexData;
            for (size_t i = 0; i < posAccessor.count; ++i)
            {
                vertexData.push_back(positions[i * 3 + 0]);
                vertexData.push_back(positions[i * 3 + 1]);
                vertexData.push_back(positions[i * 3 + 2]);
                vertexData.push_back(texcoords[i * 2 + 0]);
                vertexData.push_back(texcoords[i * 2 + 1]);
            }

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(),
                         GL_STATIC_DRAW);

            // Position
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            // TexCoord
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                                  (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            // Indices
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            GLenum indexType = idxAccessor.componentType;
            size_t indexSize =
                (indexType == GL_UNSIGNED_SHORT) ? sizeof(uint16_t) : sizeof(uint32_t);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxAccessor.count * indexSize, indices,
                         GL_STATIC_DRAW);

            glBindVertexArray(0);

            // Store for rendering
            vaos.push_back(vao);
            vbos.push_back(vbo);
            ebos.push_back(ebo);
            indexCounts.push_back(idxAccessor.count);
            indexTypes.push_back(indexType);
        }
    }

    // Load texture (assuming all primitives use the same texture for now)
    GLuint textureID = 0;
    if (!model.meshes.empty() && !model.meshes[0].primitives.empty())
    {
        const auto& primitive = model.meshes[0].primitives[0];
        if (primitive.material >= 0)
        {
            int materialIndex = primitive.material;
            const tinygltf::Material& material = model.materials[materialIndex];
            if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
            {
                int baseColorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                const tinygltf::Texture& tex = model.textures[baseColorTextureIndex];
                const tinygltf::Image& image = model.images[tex.source];

                // Upload to OpenGL
                glGenTextures(1, &textureID);
                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, image.image.data());
                glGenerateMipmap(GL_TEXTURE_2D);

                // Set texture parameters
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
        }
    }

    // Generate terrain
    int terrainWidth = 100;
    int terrainHeight = 100;
    float terrainScale = 10.0f;

    std::vector<TerrainVertex> terrainVertices =
        generateTerrainVertices(terrainWidth, terrainHeight, terrainScale);
    std::vector<unsigned int> terrainIndices = generateTerrainIndices(terrainWidth, terrainHeight);

    // Create terrain VAO and VBO
    GLuint terrainVAO, terrainVBO, terrainEBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    glBindVertexArray(terrainVAO);

    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, terrainVertices.size() * sizeof(TerrainVertex),
                 terrainVertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          (void*)offsetof(TerrainVertex, texCoord));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, terrainIndices.size() * sizeof(unsigned int),
                 terrainIndices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // Create a procedural terrain texture
    GLuint grassTexture;
    glGenTextures(1, &grassTexture);
    glBindTexture(GL_TEXTURE_2D, grassTexture);

    // Generate a procedural grass texture
    const int texSize = 256;
    std::vector<unsigned char> textureData(texSize * texSize * 4);

    FastNoiseLite textureNoise;
    textureNoise.SetSeed(42);
    textureNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    textureNoise.SetFrequency(0.1f);
    textureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    textureNoise.SetFractalOctaves(3);

    for (int y = 0; y < texSize; ++y)
    {
        for (int x = 0; x < texSize; ++x)
        {
            float noise = textureNoise.GetNoise((float)x, (float)y);
            int index = (y * texSize + x) * 4;

            // Create grass-like texture with variation
            unsigned char green = 100 + (unsigned char)(noise * 50);
            unsigned char red = 20 + (unsigned char)(noise * 30);
            unsigned char blue = 20 + (unsigned char)(noise * 20);

            textureData[index] = red; // R
            textureData[index + 1] = green; // G
            textureData[index + 2] = blue; // B
            textureData[index + 3] = 255; // A
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 textureData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Initialize grass manager
    grassManager = new GrassManager();
    grassManager->init(&terrainVertices, terrainWidth, terrainHeight, terrainScale);

    // Initialize geometry shader version (placeholder)
    grassManagerGeom = new GrassManagerGeom();
    grassManagerGeom->init(&terrainVertices, terrainWidth, terrainHeight, terrainScale);

    // float rotationX = 0.0f;
    // float rotationY = 0.0f;

    float deltaTime = 0.f;
    float lastFrame = 0.f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::Text("Delta Time: %.3f", deltaTime);
        ImGui::End();

        glfwPollEvents();

        bool moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        bool jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

        player.processInput(deltaTime, moveForward, moveBackward, moveLeft, moveRight, jump, yaw);
        player.update(deltaTime);

        // Regenerate terrain with R key
        static bool rKeyPressed = false;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed)
        {
            rKeyPressed = true;
            // Regenerate terrain with new seed
            std::vector<TerrainVertex> newTerrainVertices =
                generateTerrainVertices(terrainWidth, terrainHeight, terrainScale);
            glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
            glBufferData(GL_ARRAY_BUFFER, newTerrainVertices.size() * sizeof(TerrainVertex),
                         newTerrainVertices.data(), GL_STATIC_DRAW);
            std::cout << "Terrain regenerated with new seed!" << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE)
        {
            rKeyPressed = false;
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        shader.setInt("texture1", 0);

        glm::mat4 modelMat = glm::mat4(1.0f);
        // Position the character at the player's location
        modelMat = glm::translate(modelMat, player.getPosition());

        // Rotate the player model to face the camera's forward direction
        // The camera's forward direction is determined by the yaw angle
        float playerRotationY =
            -yaw + 90.0f; // Invert yaw and add 90 degrees to align with camera forward
        modelMat =
            glm::rotate(modelMat, glm::radians(playerRotationY), glm::vec3(0.0f, 1.0f, 0.0f));

        // Calculate camera position based on mouse angles
        glm::vec3 playerPos = player.getPosition();
        glm::vec3 camPos;

        // Calculate camera offset based on yaw and pitch
        float horizontalDistance = cameraDistance * cos(glm::radians(pitch));
        float verticalOffset = cameraDistance * sin(glm::radians(pitch));

        // Calculate camera position
        camPos.x = playerPos.x - horizontalDistance * cos(glm::radians(yaw));
        camPos.y = playerPos.y + cameraHeight + verticalOffset;
        camPos.z = playerPos.z - horizontalDistance * sin(glm::radians(yaw));

        glm::mat4 view = glm::lookAt(camPos, playerPos, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 projection =
            glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // Draw all loaded meshes
        for (size_t i = 0; i < vaos.size(); ++i)
        {
            // Apply local transform first, then world transform
            glm::mat4 finalModelMat = modelMat * localTransforms[i];
            shader.setMat4("model", finalModelMat);

            glBindVertexArray(vaos[i]);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCounts[i]), indexTypes[i], 0);
            glBindVertexArray(0);
        }

        // Draw terrain
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        shader.setInt("texture1", 0);

        glm::mat4 terrainModel = glm::mat4(1.0f);
        terrainModel =
            glm::translate(terrainModel, glm::vec3(-50.0f, 0.0f, -50.0f)); // Center the terrain
        shader.setMat4("model", terrainModel);

        glBindVertexArray(terrainVAO);
        glDrawElements(GL_TRIANGLES, terrainIndices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Draw grass using GrassManager
        grassManager->setCameraPosition(camPos);
        grassManager->setViewMatrix(view);
        grassManager->setProjectionMatrix(projection);
        grassManager->draw();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
    }

    // Cleanup
    for (size_t i = 0; i < vaos.size(); ++i)
    {
        glDeleteVertexArrays(1, &vaos[i]);
        glDeleteBuffers(1, &vbos[i]);
        glDeleteBuffers(1, &ebos[i]);
    }
    if (textureID != 0)
    {
        glDeleteTextures(1, &textureID);
    }

    // Cleanup terrain
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);
    glDeleteTextures(1, &grassTexture);

    // Cleanup grass managers
    if (grassManager)
    {
        grassManager->cleanup();
        delete grassManager;
    }
    if (grassManagerGeom)
    {
        grassManagerGeom->cleanup();
        delete grassManagerGeom;
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
