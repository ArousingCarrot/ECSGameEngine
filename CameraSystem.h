#pragma once

#include "ISystem.h"

class Window;

class CameraSystem : public ISystem {
public:
    explicit CameraSystem(Window* window);
    void Update(float dt) override;

private:
    Window* mWindow = nullptr;
};
