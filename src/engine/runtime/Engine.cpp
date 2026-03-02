#include "Engine.h"
#include "ECS.h"
#include "Window.h"
#include "Diagnostics.h"
#include "GpuTimers.h"
#include "Platform.h"
#include "InputState.h"
#include "InputBackend.h"
#include "RenderDeviceGL.h"
#include "EditorUI.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <SDL3/SDL.h>
#include <iostream>

Engine::Engine(Window* window)
    : mWindow(window)
{
    mPerfFreq = SDL_GetPerformanceFrequency();
    mLastPerfCounter = SDL_GetPerformanceCounter();
    mFrameIndex = 0;

    // Initialize GL loader + baseline GL state
    mRenderDevice = std::make_unique<RenderDeviceGL>();
    RenderDeviceGLInfo rdInfo{};
    rdInfo.vsync = true;

    if (mWindow && mRenderDevice->initialize(*mWindow, rdInfo))
    {
        mGraphicsInitialized = true;
    }
    else
    {
        mGraphicsInitialized = false;
        std::cerr << "[Engine] RenderDeviceGL initialization failed.\n";
        return;
    }

    // Diagnostics GPU pool requires GL entry points to be loaded first.
    diag::BindGlobalGpuPool();
    diag::Diagnostics::I().setOverlayVisible(true);
}

Engine::~Engine() = default;

SystemManager& Engine::getSystemManager() {
    return mECS.GetSystemManager();
}

bool Engine::PollEvents()
{
    // Legacy helper
    bool running = true;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            running = false;
        }
    }
    return running;
}

void Engine::Update(float dt)
{
    // 0) Pump SDL events
    mInputBackend.pumpEvents(
        mWindow ? mWindow->getSDLWindow() : nullptr,
        mInputState,
        [](const SDL_Event& ev) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
        });

    // 1) Diagnostics frame begin
    diag::Diagnostics::I().beginFrame(mFrameIndex);
    {
        AssetMemorySummary mem = mAssets.SummarizeMemory();
        diag::EngineMemory em{};
        em.textures = mem.textures;
        em.buffers = mem.buffers;
        em.meshes = mem.meshes;
        em.other = mem.other;
        diag::Diagnostics::I().publishEngineMemory(em);
    }

    // 2) Begin ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // 3) Run ECS systems first
    mECS.Update(dt);

    // 4) Build UI windows
    editor::DrawEditorUI();
    diag::Diagnostics::I().drawOverlay();

    // 5) End ImGui frame + render UI to the default framebuffer + present
    ImGui::Render();

    int dw = 0, dh = 0;
    if (mWindow) {
        SDL_GetWindowSizeInPixels(mWindow->getSDLWindow(), &dw, &dh);
    }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);

    // Clear the main window
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0.03f, 0.035f, 0.045f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (mWindow) {
        mWindow->update();
    }

    // 6) CPU frame timing
    const std::uint64_t now = SDL_GetPerformanceCounter();

    double frameSeconds = 0.0;
    if (mPerfFreq != 0) {
        frameSeconds = static_cast<double>(now - mLastPerfCounter) /
            static_cast<double>(mPerfFreq);
    }
    mLastPerfCounter = now;

    const double cpuMs = frameSeconds * 1000.0;
    const double fps = (frameSeconds > 0.0) ? (1.0 / frameSeconds) : 0.0;
    const double gpuMs = -1.0;

    diag::Diagnostics::I().endFrame(mFrameIndex, cpuMs, gpuMs, fps);
    ++mFrameIndex;
}

void Engine::Shutdown()
{
    if (mRenderDevice)
        mRenderDevice->shutdown();

    diag::Diagnostics::I().shutdown();
    plat::Shutdown();
}
