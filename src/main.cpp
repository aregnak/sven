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

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// FastNoiseLite for procedural generation
#define FASTNOISE_LITE_IMPLEMENTATION
#include "FastNoiseLite.h"

#include "camera.h"
#include "grass.h"
#include "player.h"

// Global variables
const int SCR_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// Camera variables
Camera camera(glm::vec3(0.0f, 15.0f, 15.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

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
        //std::cout << "Found mesh " << node.mesh << " at node " << nodeIndex << std::endl;
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
    static float lastX = SCR_WIDTH / 2.0f;
    static float lastY = SCR_HEIGHT / 2.0f;
    static bool firstMouse = true;

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

    camera.processMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.processMouseScroll(static_cast<float>(yoffset));
}

// Add this struct at the top, after includes
struct CameraBufferObject
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 position;
    float _pad = 0.0f; // Padding to align to 16 bytes
};

struct AnimSampler
{
    std::vector<float> input;
    std::vector<glm::vec4> output;
    std::string interpolation;
};

struct AnimChannel
{
    int targetNode;
    std::string path; // "translation", "rotation", "scale"
    AnimSampler sampler;
};

struct Animation
{
    std::string name;
    std::vector<AnimChannel> channels;
};

std::vector<Animation> animations;
float animationTime = 0.0f;

template <typename T>
std::vector<T> readAccessorVec(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    const uint8_t* data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

    std::vector<T> values(accessor.count);
    memcpy(values.data(), data, accessor.count * sizeof(T));
    return values;
}

int findKeyframe(const std::vector<float>& times, float time)
{
    for (size_t i = 0; i < times.size() - 1; ++i)
        if (time >= times[i] && time <= times[i + 1])
            return i;
    return 0;
}

glm::vec4 interpolate(const std::vector<float>& times, const std::vector<glm::vec4>& values,
                      float time, const std::string& path)
{
    if (times.empty())
        return values[0];
    if (time <= times.front())
        return values.front();
    if (time >= times.back())
        return values.back();

    int index = findKeyframe(times, time);
    float t0 = times[index];
    float t1 = times[index + 1];
    float alpha = (time - t0) / (t1 - t0);

    const glm::vec4& v0 = values[index];
    const glm::vec4& v1 = values[index + 1];

    if (path == "rotation")
    {
        return glm::mix(v0, v1, alpha); // vec4, assumes w is last
    }
    else
    {
        return glm::mix(v0, v1, alpha);
    }
}

void updateAnimation(float deltaTime, tinygltf::Model& model, std::vector<glm::mat4>& nodeMatrices,
                     std::map<int, glm::mat4> meshTransforms)
{
    if (animations.empty())
        return;

    animationTime += deltaTime;

    Animation& anim = animations[0]; // just play the first animation for now

    for (const auto& channel : anim.channels)
    {
        int nodeIndex = channel.targetNode;
        const auto& sampler = channel.sampler;

        glm::vec4 value = interpolate(sampler.input, sampler.output,
                                      fmod(animationTime, sampler.input.back()), channel.path);

        auto& node = model.nodes[nodeIndex];
        if (channel.path == "translation")
        {
            node.translation = { value.x, value.y, value.z };
        }
        else if (channel.path == "rotation")
        {
            node.rotation = { value.x, value.y, value.z, value.w };
        }
        else if (channel.path == "scale")
        {
            node.scale = { value.x, value.y, value.z };
        }
    }

    // Recompute all local node matrices
    nodeMatrices.clear();
    for (const auto& node : model.nodes)
        nodeMatrices.push_back(getNodeTransform(node)); // your existing helper

    // Recompute world transforms
    meshTransforms.clear();
    const auto& defaultScene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
    for (int rootNodeIndex : defaultScene.nodes)
        traverseScene(model, rootNodeIndex, nodeMatrices, glm::mat4(1.0f), meshTransforms);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // MSAAx4
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "sven", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    // VSYNC
    glfwSwapInterval(0);
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
            }
        }
    }

    if (!model.scenes.empty())
    {
        const auto& defaultScene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
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

    for (const auto& anim : model.animations)
    {
        Animation animOut;
        animOut.name = anim.name;

        for (const auto& channel : anim.channels)
        {
            const auto& sampler = anim.samplers[channel.sampler];
            const auto& inputAccessor = model.accessors[sampler.input];
            const auto& outputAccessor = model.accessors[sampler.output];

            AnimChannel channelOut;
            channelOut.targetNode = channel.target_node;
            channelOut.path = channel.target_path;
            channelOut.sampler.interpolation = sampler.interpolation;

            // Read input times
            {
                const auto& inputView = model.bufferViews[inputAccessor.bufferView];
                const auto& inputBuffer = model.buffers[inputView.buffer];

                const float* input = reinterpret_cast<const float*>(
                    inputBuffer.data.data() + inputView.byteOffset + inputAccessor.byteOffset);

                channelOut.sampler.input = std::vector<float>(input, input + inputAccessor.count);
            }

            // Read outputs

            const auto& outputView = model.bufferViews[outputAccessor.bufferView];
            const auto& outputBuffer = model.buffers[outputView.buffer];

            const uint8_t* outputData =
                outputBuffer.data.data() + outputView.byteOffset + outputAccessor.byteOffset;

            if (channel.target_path == "rotation")
            {
                const glm::vec4* output = reinterpret_cast<const glm::vec4*>(
                    outputBuffer.data.data() + outputView.byteOffset + outputAccessor.byteOffset);
                channelOut.sampler.output =
                    std::vector<glm::vec4>(output, output + outputAccessor.count);
            }
            else
            {
                const glm::vec3* output = reinterpret_cast<const glm::vec3*>(
                    outputBuffer.data.data() + outputView.byteOffset + outputAccessor.byteOffset);
                for (size_t i = 0; i < outputAccessor.count; ++i)
                    channelOut.sampler.output.push_back(glm::vec4(output[i], 0.0f)); // pad to vec4
            }

            animOut.channels.push_back(channelOut);
        }

        animations.push_back(animOut);
    }
    std::cout << "Animations loaded: " << animations.size() << "\n";
    for (auto& a : animations)
        std::cout << " - " << a.name << " channel count: " << a.channels.size() << "\n";

    float deltaTime = 0.f;
    float lastFrame = 0.f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Camera UBO for compute shader
    GLuint camera_ubo;
    glGenBuffers(1, &camera_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, camera_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraBufferObject), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, camera_ubo); // Bind to binding = 0

    GrassManager grassManager;
    grassManager.initialize(160000, 60.f, 60.f);

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

        player.processInput(deltaTime, moveForward, moveBackward, moveLeft, moveRight, jump,
                            camera.getYaw());
        player.update(deltaTime, 0);
        grassManager.update(deltaTime, glm::vec3(1.f, 0.f, 0.5f));

        std::vector<glm::mat4> nodeMatrices;
        nodeMatrices.clear();
        for (const auto& node : model.nodes)
        {
            nodeMatrices.push_back(getNodeTransform(node));
        }

        updateAnimation(deltaTime, model, nodeMatrices, meshTransforms);

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
            -camera.getYaw() + 90.0f; // Invert yaw and add 90 degrees to align with camera forward
        modelMat =
            glm::rotate(modelMat, glm::radians(playerRotationY), glm::vec3(0.0f, 1.0f, 0.0f));

        // Calculate camera position based on mouse angles
        glm::vec3 playerPos = player.getPosition();
        //glm::vec3 camPos;

        // Calculate camera offset based on yaw and pitch
        // float horizontalDistance = cameraDistance * cos(glm::radians(pitch));
        // float verticalOffset = cameraDistance * sin(glm::radians(pitch));

        // Calculate camera position
        // camPos.x = playerPos.x - horizontalDistance * cos(glm::radians(yaw));
        // camPos.y = playerPos.y + cameraHeight + verticalOffset;
        // camPos.z = playerPos.z - horizontalDistance * sin(glm::radians(yaw));

        // If camera is below terrain, move it closer to the player until it's above
        float minCameraHeight = 0.5f; // Minimum height above terrain
        // float terrainY = grassManager->getTerrainHeight(camPos.x, camPos.z);

        // If camera is below terrain, move it closer to the player until it's above
        // if (camPos.y < terrainY + minCameraHeight)
        // {
        //     glm::vec3 toPlayer = glm::normalize(playerPos - camPos);
        //     float step = 0.1f; // How much to move per iteration (tune as needed)
        //     int maxSteps = 100; // Prevent infinite loop

        //     for (int i = 0; i < maxSteps && camPos.y < terrainY + minCameraHeight; ++i)
        //     {
        //         camPos += toPlayer * step;
        //         // terrainY = grassManager->getTerrainHeight(camPos.x, camPos.z);
        //     }
        // }

        camera.updatePosition(player.getPosition());

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection =
            glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        // Update camera UBO for compute shader
        CameraBufferObject camubo;
        camubo.view = view;
        camubo.proj = projection;
        camubo.position = camera.getPosition();
        glBindBuffer(GL_UNIFORM_BUFFER, camera_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraBufferObject), &camubo);

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

        shader.setInt("texture1", 0);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspectRatio = static_cast<float>(width) / height;

        auto frustumPlanes = camera.getFrustumPlanes(aspectRatio);
        grassManager.render(view, projection, camera.getPosition(), frustumPlanes);

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

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
