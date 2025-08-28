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

#include "camera.h"
#include "grass.h"
#include "player.h"

// Global variables
const int SCR_WIDTH = 1280;
const int SCR_HEIGHT = 720;

// Camera variables
Camera camera(glm::vec3(0.0f, 15.0f, 15.0f));

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

        grassManager.update(deltaTime, glm::vec3(1.f, 0.f, 0.5f));

        std::vector<glm::mat4> nodeMatrices;
        nodeMatrices.clear();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        shader.setInt("texture1", 0);

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
        shader.setInt("texture1", 0);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspectRatio = static_cast<float>(width) / height;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
