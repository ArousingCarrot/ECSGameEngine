#include "RenderDeviceGL.h"
#include "Window.h"

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <iostream>

static std::string SafeStr(const char* s)
{
    return s ? std::string(s) : std::string{};
}

bool RenderDeviceGL::initialize(Window& window, const RenderDeviceGLInfo& info)
{
    mWindow = &window;

    if (!window.isInitialized())
    {
        std::cerr << "[RenderDeviceGL] Window is not initialized.\n";
        return false;
    }

    SDL_Window* sdlWin = window.getSDLWindow();
    SDL_GLContext ctx = window.getGLContext();

    if (!sdlWin || !ctx)
    {
        std::cerr << "[RenderDeviceGL] Missing SDL_Window or GL context.\n";
        return false;
    }

    // Ensure context is current on this thread.
    if (SDL_GL_GetCurrentContext() != ctx)
    {
        if (SDL_GL_MakeCurrent(sdlWin, ctx) != 0)
        {
            std::cerr << "[RenderDeviceGL] SDL_GL_MakeCurrent failed: "
                << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
            return false;
        }
    }

    // Load OpenGL entry points (GLAD).
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        std::cerr << "[RenderDeviceGL] gladLoadGLLoader failed.\n";
        return false;
    }

    // Capabilities / strings
    {
        GLint maj = 0, min = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &maj);
        glGetIntegerv(GL_MINOR_VERSION, &min);

        mCaps.major = (int)maj;
        mCaps.minor = (int)min;
        mCaps.vendor = SafeStr((const char*)glGetString(GL_VENDOR));
        mCaps.renderer = SafeStr((const char*)glGetString(GL_RENDERER));
        mCaps.version = SafeStr((const char*)glGetString(GL_VERSION));
    }

    // Minimal baseline GL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    setVsync(info.vsync);

    mInitialized = true;
    return true;
}

void RenderDeviceGL::shutdown()
{
    mInitialized = false;
    mWindow = nullptr;
}

void RenderDeviceGL::setVsync(bool enabled)
{
    mVsync = enabled;

    if (!mWindow || !mWindow->getSDLWindow())
        return;

    // 1 = vsync, 0 = immediate
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) != 1)
    {
        std::cerr << "[RenderDeviceGL] SDL_GL_SetSwapInterval(" << (enabled ? 1 : 0)
            << ") warning: " << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
    }
}
