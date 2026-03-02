#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <memory>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

class Mesh {
public:
    Mesh();
    Mesh(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
    Mesh(const Mesh& other);
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(const Mesh& other);
    Mesh& operator=(Mesh&& other) noexcept;
    ~Mesh();

    void SetupMesh();
    void Draw() const;

    const std::vector<Vertex>& GetVertices() const { return vertices_; }
    const std::vector<std::uint32_t>& GetIndices() const { return indices_; }

    std::uint32_t GetIndexCount() const { return (std::uint32_t)indices_.size(); }
    std::uint32_t GetVertexCount() const { return (std::uint32_t)vertices_.size(); }

    GLuint GetVAO() const { return VAO_; }
    GLuint GetVBO() const { return VBO_; }
    GLuint GetEBO() const { return EBO_; }

private:
    std::vector<Vertex> vertices_;
    std::vector<std::uint32_t> indices_;

    GLuint VAO_ = 0;
    GLuint VBO_ = 0;
    GLuint EBO_ = 0;

    bool initialized_ = false;

    void DestroyGL();
    void CopyFrom(const Mesh& other);
};
