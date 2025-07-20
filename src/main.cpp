#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <map>
#include "shader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "player.h"

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

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
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
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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

    // bool ret = loader.LoadASCIIFromFile(&model, &err, &warn,
    //                                     "Assets/Environment/gltf/Tree_1_A_Color1.gltf");

    // if (!warn.empty())
    //     std::cout << "Warn: " << warn << std::endl;
    // if (!err.empty())
    //     std::cerr << "Err: " << err << std::endl;

    // if (!ret)
    // {
    //     std::cerr << "Failed to load glTF" << std::endl;
    //     return -1;
    // }

    // Player model
    Player player(glm::vec3(0.0f, 0.0f, 15.0f));

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

    // float rotationX = 0.0f;
    // float rotationY = 0.0f;

    float deltaTime = 0.f;
    float lastFrame = 0.f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        bool moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        bool jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

        player.processInput(deltaTime, moveForward, moveBackward, moveLeft, moveRight, jump);
        player.update(deltaTime);

        // if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        // {
        //     rotationX += 0.02f;
        // }
        // if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        // {
        //     rotationY += 0.02f;
        // }
        // if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        // {
        //     rotationX -= 0.02f;
        // }
        // if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        // {
        //     rotationY -= 0.02f;
        // }

        // glm::mat4 modelMat = glm::mat4(1.0f); // Identity, or add rotation/translation if you want

        // glm::mat4 modelMat = glm::mat4(1.0f);
        // modelMat = glm::rotate(modelMat, 0.f, glm::vec3(1.0f, 0.0f, 0.0f)); // X axis
        // modelMat = glm::rotate(modelMat, 0.f, glm::vec3(0.0f, 1.0f, 0.0f)); // Y axis

        // glm::mat4 view =
        //     glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f)); // Move camera back
        // glm::mat4 projection =
        //     glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

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

        glm::vec3 camPos = player.getPosition() +
                           glm::vec3(0.0f, 2.0f, 5.0f); // Camera behind and slightly above player
        glm::vec3 camTarget = player.getPosition(); // Camera looks at the player
        glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));

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

        glBindVertexArray(0);

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

    glfwTerminate();
    return 0;
}
