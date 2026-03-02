#include "InputSystem.h"
#include "Window.h"
#include "RenderState.h"
#include "AppState.h"
#include "Diagnostics.h"
#include "InputState.h"
#include "imgui/imgui.h"
#include "EditorUI.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

InputSystem::InputSystem(Window* window, plat::InputState& input)
    : mWindow(window)
    , mInput(input)
{
    AppState::Paused = false;
    setMouseCapture(true);
    updateViewProj();
}

void InputSystem::setMouseCapture(bool enabled)
{
    mouseCaptured = enabled;
    SDL_SetWindowRelativeMouseMode(mWindow->getSDLWindow(), enabled);
}

void InputSystem::updateViewProj()
{
    const float yawRad = glm::radians(yaw);
    const float pitchRad = glm::radians(pitch);

    glm::vec3 front;
    front.x = std::cos(yawRad) * std::cos(pitchRad);
    front.y = std::sin(pitchRad);
    front.z = std::sin(yawRad) * std::cos(pitchRad);
    front = glm::normalize(front);

    const glm::vec3 up(0.f, 1.f, 0.f);

    RenderState::View = glm::lookAt(camPos, camPos + front, up);

    auto sv = editor::GetSceneViewportInfo();
    int dw = sv.pixelW, dh = sv.pixelH;
    if (dw <= 0 || dh <= 0)
        SDL_GetWindowSizeInPixels(mWindow->getSDLWindow(), &dw, &dh);
    const float aspect = (dh > 0) ? float(dw) / float(dh) : 1.0f;
    RenderState::Projection =
        glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);

    RenderState::HasCamera = true;
}

void InputSystem::tryConsumeSceneBounds()
{
    float c[3] = { 0.0f, 0.0f, 0.0f };
    float r = 1.0f;

    if (editor::ConsumeSceneBoundsUpdate(c, r))
    {
        mHasSceneBounds = true;
        mSceneBoundsCenter[0] = c[0];
        mSceneBoundsCenter[1] = c[1];
        mSceneBoundsCenter[2] = c[2];
        mSceneBoundsRadius = r;

        if (!mDidAutoFrame)
        {
            frameToBounds(mSceneBoundsCenter, mSceneBoundsRadius);
            mDidAutoFrame = true;
        }
    }

    if (editor::ConsumeFrameRequest())
    {
        if (!mHasSceneBounds)
        {
            if (editor::GetSceneBounds(c, r))
            {
                mHasSceneBounds = true;
                mSceneBoundsCenter[0] = c[0];
                mSceneBoundsCenter[1] = c[1];
                mSceneBoundsCenter[2] = c[2];
                mSceneBoundsRadius = r;
            }
        }

        frameToBounds(mHasSceneBounds ? mSceneBoundsCenter : c,
            mHasSceneBounds ? mSceneBoundsRadius : 1.0f);
    }
}

void InputSystem::frameToBounds(const float center[3], float radius)
{
    const glm::vec3 c(center[0], center[1], center[2]);
    const float r = (radius > 1e-4f) ? radius : 1.0f;

    const float fovy = glm::radians(45.0f);
    float dist = r / std::tan(fovy * 0.5f);
    dist = dist * 1.35f + r * 0.25f;

    const glm::vec3 fromCenter = glm::normalize(glm::vec3(0.0f, 0.25f, 1.0f));
    camPos = c + fromCenter * dist;

    const glm::vec3 toTarget = glm::normalize(c - camPos);
    pitch = glm::degrees(std::asin(glm::clamp(toTarget.y, -1.0f, 1.0f)));
    yaw = glm::degrees(std::atan2(toTarget.z, toTarget.x));

    updateViewProj();
}

void InputSystem::Update(float dt)
{
    ImGuiIO& io = ImGui::GetIO();

    // --- 1) Handle quit ---
    if (mInput.quitRequested) {
        mWindow->close();
        AppState::ShouldQuit = true;
        return;
    }

    // --- 2) ESC: pause toggle ---
    if (!editor::WantsTextInput()) {
        const size_t escIdx = static_cast<size_t>(plat::Key::Escape);
        if (mInput.keyPressed[escIdx]) {
            bool newPaused = !AppState::Paused.load();
            AppState::Paused = newPaused;
            setMouseCapture(!newPaused);
            std::cerr << "[Input] Pause: "
                << (newPaused ? "ON (mouse free)" : "OFF (mouse captured)")
                << "\n";
        }
    }

    // --- 3) F1: diagnostics overlay toggle  ---
    {
        const size_t f1Idx = static_cast<size_t>(plat::Key::F1);
        if (mInput.keyPressed[f1Idx]) {
            diag::Diagnostics::I().toggleOverlay();
        }
    }

    // Scene bounds updates + frame requests.
    tryConsumeSceneBounds();

    // Frame model (R)
    if (!editor::WantsTextInput())
    {
        const size_t rIdx = static_cast<size_t>(plat::Key::R);
        if (mInput.keyPressed[rIdx])
        {
            float c[3] = { 0.0f, 0.0f, 0.0f };
            float r = 1.0f;

            if (editor::GetSceneBounds(c, r))
            {
                mHasSceneBounds = true;
                mSceneBoundsCenter[0] = c[0];
                mSceneBoundsCenter[1] = c[1];
                mSceneBoundsCenter[2] = c[2];
                mSceneBoundsRadius = r;
            }

            frameToBounds(mHasSceneBounds ? mSceneBoundsCenter : c,
                mHasSceneBounds ? mSceneBoundsRadius : 1.0f);
        }
    }

    // --- 4) Mouse click to resume when paused ---
    if (AppState::Paused && editor::ConsumeSceneClick()) {
        AppState::Paused = false;
        setMouseCapture(true);
        std::cerr << "[Input] Scene click -> resume & capture\n";
    }

    // --- 5) Mouse look  ---
    if (mouseCaptured && !AppState::Paused) {
        float dx = static_cast<float>(mInput.mouseDeltaX);
        float dy = static_cast<float>(mInput.mouseDeltaY);

        if (dx != 0.0f || dy != 0.0f) {
            yaw += dx * mouseSensitivity;
            pitch -= dy * mouseSensitivity;
            if (pitch > 89.f)  pitch = 89.f;
            if (pitch < -89.f) pitch = -89.f;
        }
    }

    // --- 6) Keyboard movement ---
    if (!AppState::Paused) {
        auto keyDown = [&](plat::Key k) -> bool {
            return mInput.keyDown[static_cast<size_t>(k)];
            };

        const float yawRad = glm::radians(yaw);
        const float pitchRad = glm::radians(pitch);

        glm::vec3 front;
        front.x = std::cos(yawRad) * std::cos(pitchRad);
        front.y = std::sin(pitchRad);
        front.z = std::sin(yawRad) * std::cos(pitchRad);
        front = glm::normalize(front);

        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.f, 1.f, 0.f)));
        glm::vec3 up = glm::normalize(glm::cross(right, front));

        const float vel = speed * dt;

        if (keyDown(plat::Key::W)) camPos += front * vel;
        if (keyDown(plat::Key::S)) camPos -= front * vel;
        if (keyDown(plat::Key::A)) camPos -= right * vel;
        if (keyDown(plat::Key::D)) camPos += right * vel;
        if (keyDown(plat::Key::E)) camPos += up * vel;
        if (keyDown(plat::Key::Q)) camPos -= up * vel;
    }

    // --- 7) Publish camera state ---
    updateViewProj();
}
