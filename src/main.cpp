#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "shader.h" // Custom shader class to compile and link shaders

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Voxel Cube", NULL, NULL);
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

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn,
                                        "Assets/Environment/gltf/Tree_1_A_Color1.gltf");

    if (!warn.empty())
        std::cout << "Warn: " << warn << std::endl;
    if (!err.empty())
        std::cerr << "Err: " << err << std::endl;

    if (!ret)
    {
        std::cerr << "Failed to load glTF" << std::endl;
        return -1;
    }

    // Assume first mesh/primitive
    const tinygltf::Mesh& mesh = model.meshes[0];
    const tinygltf::Primitive& primitive = mesh.primitives[0];

    // Positions
    const tinygltf::Accessor& posAccessor =
        model.accessors[primitive.attributes.find("POSITION")->second];
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
    const tinygltf::Accessor& texAccessor =
        model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
    const tinygltf::BufferView& texView = model.bufferViews[texAccessor.bufferView];
    const tinygltf::Buffer& texBuffer = model.buffers[texView.buffer];
    const float* texcoords = reinterpret_cast<const float*>(
        &(texBuffer.data[texView.byteOffset + texAccessor.byteOffset]));

    // OpenGL buffers
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    GLenum indexType = idxAccessor.componentType; // e.g., GL_UNSIGNED_SHORT
    size_t indexSize = (indexType == GL_UNSIGNED_SHORT) ? sizeof(uint16_t) : sizeof(uint32_t);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxAccessor.count * indexSize, indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    // Load the texture image from glTF and upload to OpenGL
    int materialIndex = primitive.material;
    const tinygltf::Material& material = model.materials[materialIndex];
    int baseColorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
    const tinygltf::Texture& tex = model.textures[baseColorTextureIndex];
    const tinygltf::Image& image = model.images[tex.source];

    // Upload to OpenGL
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.image.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set texture parameters (optional, but recommended)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    float rotationX = 0.0f;
    float rotationY = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            rotationX += 0.02f;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            rotationY += 0.02f;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            rotationX -= 0.02f;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            rotationY -= 0.02f;
        }

        // glm::mat4 modelMat = glm::mat4(1.0f); // Identity, or add rotation/translation if you want

        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::rotate(modelMat, rotationX, glm::vec3(1.0f, 0.0f, 0.0f)); // X axis
        modelMat = glm::rotate(modelMat, rotationY, glm::vec3(0.0f, 1.0f, 0.0f)); // Y axis

        glm::mat4 view =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f)); // Move camera back
        glm::mat4 projection =
            glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        shader.setMat4("model", modelMat);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        shader.setInt("texture1", 0); // Make sure your shader has a setInt method

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, idxAccessor.count, indexType, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
    }

    glfwTerminate();
    return 0;
}
