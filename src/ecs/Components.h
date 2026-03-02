#pragma once

#include <glm/glm.hpp>
#include <memory>

struct TextureAsset;
struct ShaderAsset;
struct MeshAsset;

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

struct MeshRenderer {
    std::shared_ptr<MeshAsset>  mesh;
    std::shared_ptr<ShaderAsset> shader;
    std::shared_ptr<TextureAsset> texture;
};

struct CameraComponent {
    float fovY = glm::radians(45.0f);
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
};
