#pragma once
#include <glm/glm.hpp>

namespace RenderState {
    inline glm::mat4 View = glm::mat4(1.0f);
    inline glm::mat4 Projection = glm::mat4(1.0f);
    inline bool      HasCamera = false;
}
