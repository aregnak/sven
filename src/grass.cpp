#include "grass.h"

#include <random>
#include <vector>
#include <GLFW/glfw3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <iostream>
#include <cstring>

// Utility to print OpenGL errors
void printGLErrors(const char* context)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in " << context << ": " << err << std::endl;
    }
}

struct NumBlades
{
    std::uint32_t vertexCount = 5;
    std::uint32_t instanceCount = 1;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
};

namespace
{

std::vector<Blade> generate_blades()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> orientation_dis(0, glm::pi<float>());
    std::uniform_real_distribution<float> dis(-1, 1);

    std::vector<Blade> blades;
    // Generate grass blades using jittered stratified sampling
    for (int i = -32; i < 32; ++i)
    {
        for (int j = -32; j < 32; ++j)
        {
            const float x = static_cast<float>(i) / 10 - 1 + dis(gen) * 0.1f;
            const float y = static_cast<float>(j) / 10 - 1 + dis(gen) * 0.1f;

            const float blade_height = glm::simplex(glm::vec2(x, y)) * 0.5f + 0.7f;

            blades.emplace_back(glm::vec4(x, 0, y, orientation_dis(gen)),
                                glm::vec4(x, blade_height, y, blade_height),
                                glm::vec4(x, blade_height, y, 0.1f),
                                glm::vec4(0, blade_height, 0, 0.7f + dis(gen) * 0.3f));
        }
    }
    return blades;
}

} // anonymous namespace

void printOpenGLVersion()
{
    const GLubyte* version = glGetString(GL_VERSION);
    std::cout << "OpenGL version: " << (version ? reinterpret_cast<const char*>(version) : "(null)")
              << std::endl;
}

void Grass::init()
{
    const std::vector<Blade> blades = generate_blades();
    blades_count_ = static_cast<GLuint>(blades.size());

    glPatchParameteri(GL_PATCH_VERTICES, 1);
    printGLErrors("glPatchParameteri");

    glGenVertexArrays(1, &grass_vao_);
    glBindVertexArray(grass_vao_);
    printGLErrors("glGenVertexArrays/glBindVertexArray");

    unsigned int grass_input_buffer = 0;
    glGenBuffers(1, &grass_input_buffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, grass_input_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizei>(blades.size() * sizeof(Blade)),
                 blades.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, grass_input_buffer);
    printGLErrors("input buffer setup");

    unsigned int grass_output_buffer = 0;
    glGenBuffers(1, &grass_output_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, grass_output_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizei>(blades.size() * sizeof(Blade)),
                 nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, grass_output_buffer);
    printGLErrors("output buffer setup");

    NumBlades numBlades;
    unsigned int grass_indirect_buffer = 0;
    glGenBuffers(1, &grass_indirect_buffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, grass_indirect_buffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(NumBlades), &numBlades, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, grass_output_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, grass_indirect_buffer);
    printGLErrors("indirect buffer setup");

    // v0 attribute
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Blade),
                          reinterpret_cast<void*>(offsetof(Blade, v0)));
    glEnableVertexAttribArray(0);

    // v1 attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Blade),
                          reinterpret_cast<void*>(offsetof(Blade, v1)));
    glEnableVertexAttribArray(1);

    // v2 attribute
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Blade),
                          reinterpret_cast<void*>(offsetof(Blade, v2)));
    glEnableVertexAttribArray(2);

    // dir attribute
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Blade),
                          reinterpret_cast<void*>(offsetof(Blade, up)));
    glEnableVertexAttribArray(3);
    printGLErrors("vertex attrib setup");

    try
    {
        grass_compute_shader_ =
            ShaderBuilder{}.load("shaders/grass.comp.glsl", Shader::Type::Compute).build();
        grass_compute_shader_.use();
        printGLErrors("compute shader setup");

        grass_shader_ = ShaderBuilder{}
                            .load("shaders/grass.vert.glsl", Shader::Type::Vertex)
                            .load("shaders/grass.tesc.glsl", Shader::Type::TessControl)
                            .load("shaders/grass.tese.glsl", Shader::Type::TessEval)
                            .load("shaders/grass.frag.glsl", Shader::Type::Fragment)
                            .build();
        printGLErrors("graphics shader setup");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Shader error: " << e.what() << std::endl;
    }
}

void Grass::update(float delta_time)
{
    computeDispatched_ = false;
    std::cout << "blades_count_: " << blades_count_ << std::endl;
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    std::cout << "Current program before compute: " << currentProgram << std::endl;
    // Check if the compute shader program is valid and linked
    GLint linkStatus = 0;
    glGetProgramiv(grass_compute_shader_.id(), GL_LINK_STATUS, &linkStatus);
    std::cout << "Compute shader link status: " << linkStatus << std::endl;

    // Print SSBO and indirect buffer bindings
    GLint ssbo1 = 0, ssbo2 = 0, ssbo3 = 0;
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 1, &ssbo1);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 2, &ssbo2);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 3, &ssbo3);
    GLint indirectBuffer = 0;
    glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirectBuffer);
    std::cout << "SSBO1 (input): " << ssbo1 << ", SSBO2 (output): " << ssbo2
              << ", SSBO3 (indirect): " << ssbo3 << std::endl;
    std::cout << "Indirect buffer: " << indirectBuffer << std::endl;

    // Print UBO0 binding
    GLint ubo0 = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &ubo0);
    std::cout << "UBO0 (camera): " << ubo0 << std::endl;

    if (blades_count_ > 0)
    {
        grass_compute_shader_.use();
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
        std::cout << "Current program after use(): " << currentProgram << std::endl;
        // set uniforms...
        glDispatchCompute(blades_count_, 1, 1);
        GLenum err = glGetError();
        if (err != GL_NO_ERROR)
        {
            std::cerr << "OpenGL error after glDispatchCompute: " << err << std::endl;
        }
        printGLErrors("glDispatchCompute");
        computeDispatched_ = true;
    }
}

void Grass::render()
{
    if (computeDispatched_)
    {
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        printGLErrors("glMemoryBarrier");
    }

    glBindVertexArray(grass_vao_);
    printGLErrors("glBindVertexArray");
    grass_shader_.use();
    printGLErrors("graphics shader use");
    glDrawArraysIndirect(GL_PATCHES, reinterpret_cast<void*>(0));
    printGLErrors("glDrawArraysIndirect");
}
