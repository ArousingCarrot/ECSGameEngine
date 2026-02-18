#pragma once

#include "window.h"
#include "Shader.h"
#include "Camera.h"
#include "Mesh.h"

#include <glad/glad.h>

namespace Renderer {
    bool InitializeOpenGL(Window& window);

    void GetAABB(const Mesh& mesh, glm::vec3& outMin, glm::vec3& outMax);

    void RenderScene(Window& window, Shader& shader, Camera& camera,
        float gameTime, const Mesh& mesh, GLuint nullTexture);

    void RenderSceneNoSwap(Window& window, Shader& shader, Camera& camera,
        float gameTime, const Mesh& mesh, GLuint nullTexture);

}
