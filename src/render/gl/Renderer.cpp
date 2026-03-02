#include "GraphicsGL.h"
#include "Renderer.h"
#include "window.h"
#include "Shader.h"
#include "Camera.h"
#include "Mesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <iostream>

namespace Renderer {

    void GetAABB(const Mesh& mesh, glm::vec3& outMin, glm::vec3& outMax)
    {
        const auto& verts = mesh.GetVertices();
        if (verts.empty()) {
            outMin = glm::vec3(0.0f);
            outMax = glm::vec3(0.0f);
            return;
        }

        using std::numeric_limits;
        outMin = glm::vec3(numeric_limits<float>::max());
        outMax = glm::vec3(numeric_limits<float>::lowest());

        for (const Vertex& v : verts) {
            outMin = glm::min(outMin, v.position);
            outMax = glm::max(outMax, v.position);
        }
    }

    // OpenGL init
    bool InitializeOpenGL(Window& window)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        return true;
    }

    // Shared helper – sets matrices on a shader
    static void setMatrices(Shader& sh, Window& win, Camera& cam, float time)
    {
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            float(win.getWidth()) / win.getHeight(),
            0.1f, 100.0f);
        glm::mat4 view = cam.GetViewMatrix();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(50.0f),
            glm::vec3(0.5f, 1.0f, 0.0f));
        sh.setMat4("projection", projection);
        sh.setMat4("view", view);
        sh.setMat4("model", model);
    }

    // Render + swap
    void RenderScene(Window& win, Shader& sh, Camera& cam, float t,
        const Mesh& mesh, GLuint nullTex)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sh.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, nullTex);
        sh.setInt("faceTexture", 0);

        setMatrices(sh, win, cam, t);
        mesh.Draw();
        win.update();
    }

    // Render without swap (for multi-pass pipeline)
    void RenderSceneNoSwap(Window& win, Shader& sh, Camera& cam, float t,
        const Mesh& mesh, GLuint nullTex)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sh.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, nullTex);
        sh.setInt("faceTexture", 0);

        setMatrices(sh, win, cam, t);
        mesh.Draw();
    }

}