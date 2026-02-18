#pragma once

#include "ISystem.h"
#include <glm/glm.hpp>

class Window;

namespace plat { struct InputState; }

class InputSystem : public ISystem {
public:
    InputSystem(Window* window, plat::InputState& input);
    void Update(float dt) override;

private:
    Window* mWindow = nullptr;
    plat::InputState& mInput;

    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 4.0f;
    float mouseSensitivity = 0.12f;

    bool mouseCaptured = false;

    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 6.0f);

    bool  mHasSceneBounds = false;
    float mSceneBoundsCenter[3] = { 0.0f, 0.0f, 0.0f };
    float mSceneBoundsRadius = 1.0f;
    bool  mDidAutoFrame = false;

    void  tryConsumeSceneBounds();
    void  frameToBounds(const float center[3], float radius);

    void setMouseCapture(bool enabled);
    void updateViewProj();
};
