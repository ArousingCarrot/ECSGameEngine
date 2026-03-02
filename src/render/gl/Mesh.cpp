#include "Mesh.h"
#include <cstring>

Mesh::Mesh() {}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices)
    : vertices_(vertices), indices_(indices) {
}

Mesh::Mesh(const Mesh& other) {
    CopyFrom(other);
}

Mesh::Mesh(Mesh&& other) noexcept {
    vertices_ = std::move(other.vertices_);
    indices_ = std::move(other.indices_);

    VAO_ = other.VAO_;
    VBO_ = other.VBO_;
    EBO_ = other.EBO_;
    initialized_ = other.initialized_;

    other.VAO_ = 0;
    other.VBO_ = 0;
    other.EBO_ = 0;
    other.initialized_ = false;
}

Mesh& Mesh::operator=(const Mesh& other) {
    if (this == &other) return *this;
    DestroyGL();
    vertices_.clear();
    indices_.clear();
    CopyFrom(other);
    return *this;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;
    DestroyGL();

    vertices_ = std::move(other.vertices_);
    indices_ = std::move(other.indices_);

    VAO_ = other.VAO_;
    VBO_ = other.VBO_;
    EBO_ = other.EBO_;
    initialized_ = other.initialized_;

    other.VAO_ = 0;
    other.VBO_ = 0;
    other.EBO_ = 0;
    other.initialized_ = false;
    return *this;
}

Mesh::~Mesh() {
    DestroyGL();
}

void Mesh::DestroyGL() {
    if (EBO_) glDeleteBuffers(1, &EBO_);
    if (VBO_) glDeleteBuffers(1, &VBO_);
    if (VAO_) glDeleteVertexArrays(1, &VAO_);
    EBO_ = 0;
    VBO_ = 0;
    VAO_ = 0;
    initialized_ = false;
}

void Mesh::CopyFrom(const Mesh& other) {
    vertices_ = other.vertices_;
    indices_ = other.indices_;
    VAO_ = 0;
    VBO_ = 0;
    EBO_ = 0;
    initialized_ = false;
}

void Mesh::SetupMesh() {
    if (initialized_) return;

    glGenVertexArrays(1, &VAO_);
    glGenBuffers(1, &VBO_);
    glGenBuffers(1, &EBO_);

    glBindVertexArray(VAO_);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(vertices_.size() * sizeof(Vertex)),
        vertices_.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        (GLsizeiptr)(indices_.size() * sizeof(std::uint32_t)),
        indices_.data(),
        GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    // tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    // bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));

    glBindVertexArray(0);
    initialized_ = true;
}

void Mesh::Draw() const {
    if (!VAO_) return;
    glBindVertexArray(VAO_);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices_.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
