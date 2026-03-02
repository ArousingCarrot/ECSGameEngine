#ifndef SHADER_H
#define SHADER_H

#include <string>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader {
public:
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();

    void use() const;

    static std::string ReadFileToString(const char* path);

    void setMat4(const std::string& name, const glm::mat4& mat) const;

    void setInt(const std::string& name, int value) const;
    void setBool(const std::string& name, bool value) const;
    void setFloat(const std::string& name, float value) const;

    void setVec2(const std::string& name, const glm::vec2& vec) const;
    void setVec3(const std::string& name, const glm::vec3& vec) const;
    void setVec4(const std::string& name, const glm::vec4& vec) const;

    GLuint getID() const { return ID; }

private:
    GLuint ID = 0;
};

#endif
