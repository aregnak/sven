#include "shader.h"
#include <glm/gtc/type_ptr.hpp>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;

        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
    }
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void checkCompilingError(unsigned int shader_id)
{
    int success;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        constexpr int max_log_size = 512;
        char info_log[max_log_size];
        glGetShaderInfoLog(shader_id, max_log_size, nullptr, static_cast<char*>(info_log));
        throw std::runtime_error{ fmt::format("Shader compilation error for shader {}: {}",
                                              shader_id, info_log) };
    }
}

void checkLinkingError(unsigned int program_id)
{
    int success;
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);
    GLchar info_log[1024];
    if (!success)
    {
        glGetProgramInfoLog(program_id, 1024, nullptr, static_cast<GLchar*>(info_log));
        throw std::runtime_error{ fmt::format("Shader linking error: {}", info_log) };
    }
}

Shader::Shader(const char* source, Shader::Type type)
    : type_{ type }
    , id_{ glCreateShader(std::underlying_type_t<Type>(type)) }
{
    glShaderSource(id_, 1, &source, nullptr);
    glCompileShader(id_);
    checkCompilingError(id_);
}

Shader::~Shader() { glDeleteShader(id_); }

Shader::Shader(Shader&& other) noexcept
    : type_{ other.type_ }
    , id_{ other.id_ }
{
    other.id_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    std::swap(id_, other.id_);
    std::swap(type_, other.type_);
    return *this;
}

// Read whole file into a string
std::string Shader::readFile(std::string_view filename)
{
    std::ifstream file{ std::string(filename) };
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + std::string(filename));
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

ShaderProgram::ShaderProgram(const std::vector<Shader>& shaders)
    : id_{ glCreateProgram() }
{
    for (const auto& shader : shaders)
    {
        glAttachShader(id_, shader.id_);
    }

    glLinkProgram(id_);
    checkLinkingError(id_);
}

void Shader::use() { glUseProgram(ID); }

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::checkCompileErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                      << infoLog << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                      << infoLog << std::endl;
        }
    }
}