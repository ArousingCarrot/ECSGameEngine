#include "Shader.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

static GLuint compileStage(GLenum type, const char* src, const char* label)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(sh, (GLsizei)log.size(), nullptr, log.data());
        std::cerr << "[Shader] Compile failed (" << label << "):\n" << log.data() << "\n";
    }
    return sh;
}

std::string Shader::ReadFileToString(const char* path)
{
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "[Shader] Failed to open file: " << path << "\n";
        return {};
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    const std::string vStr = ReadFileToString(vertexPath);
    const std::string fStr = ReadFileToString(fragmentPath);
    const char* vSrc = vStr.c_str();
    const char* fSrc = fStr.c_str();

    GLuint vs = compileStage(GL_VERTEX_SHADER, vSrc, vertexPath);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fSrc, fragmentPath);

    ID = glCreateProgram();
    glAttachShader(ID, vs);
    glAttachShader(ID, fs);
    glLinkProgram(ID);

    GLint ok = 0;
    glGetProgramiv(ID, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(ID, (GLsizei)log.size(), nullptr, log.data());
        std::cerr << "[Shader] Link failed:\n" << log.data() << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader()
{
    if (ID != 0) {
        glDeleteProgram(ID);
        ID = 0;
    }
}

void Shader::use() const
{
    glUseProgram(ID);
}

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

void Shader::setVec2(const std::string& name, const glm::vec2& value) const
{
    glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}
