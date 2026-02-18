#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0
#endif
#ifndef GL_DEBUG_OUTPUT_SYNCHRONOUS
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#endif
#ifndef GL_DONT_CARE
#define GL_DONT_CARE 0x1100
#endif
#ifndef GL_DEBUG_SEVERITY_NOTIFICATION
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#endif

#include "GraphicsGL.h"
#include "Window.h"

#include <SDL3/SDL.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <stdexcept>

#if defined(_WIN32)
#  include <windows.h>
#endif
#if defined(_WIN32)
#  define PT_GLAPIENTRY __stdcall
#else
#  define PT_GLAPIENTRY
#endif

using GLDEBUGPROC_ = void (PT_GLAPIENTRY*)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*);
using PFNGLDEBUGMESSAGECALLBACKPROC_ = void (PT_GLAPIENTRY*)(GLDEBUGPROC_ callback, const void* userParam);
using PFNGLDEBUGMESSAGECONTROLPROC_ = void (PT_GLAPIENTRY*)(GLenum source, GLenum type, GLenum severity,
    GLsizei count, const GLuint* ids, GLboolean enabled);

static void PT_GLAPIENTRY GLDebugCallback_(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    if (id == 131185 || id == 131218 || id == 131204) return;

    std::cerr << "[GL] " << (message ? message : "(null)") << "\n";
}

static void InstallGLDebugOutput_()
{
    auto cb = (PFNGLDEBUGMESSAGECALLBACKPROC_)SDL_GL_GetProcAddress("glDebugMessageCallback");
    auto ctrl = (PFNGLDEBUGMESSAGECONTROLPROC_)SDL_GL_GetProcAddress("glDebugMessageControl");

    if (!cb) {
        cb = (PFNGLDEBUGMESSAGECALLBACKPROC_)SDL_GL_GetProcAddress("glDebugMessageCallbackARB");
        ctrl = (PFNGLDEBUGMESSAGECONTROLPROC_)SDL_GL_GetProcAddress("glDebugMessageControlARB");
    }

    if (!cb) return;

    glEnable(GL_DEBUG_OUTPUT);

    cb(GLDebugCallback_, nullptr);

    if (ctrl) {
        ctrl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
}

Window* Window::s_primaryWindow = nullptr;

static void PrintLoadedSDL3Path()
{
#if defined(_WIN32)
    HMODULE mod = GetModuleHandleA("SDL3.dll");
    if (!mod) {
        std::cerr << "[SDL DIAG] SDL3.dll not yet loaded (GetModuleHandleA returned null)\n";
        return;
    }
    char path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(mod, path, MAX_PATH);
    if (n == 0) {
        std::cerr << "[SDL DIAG] GetModuleFileNameA failed for SDL3.dll\n";
    }
    else {
        std::cerr << "[SDL DIAG] Using SDL3.dll at: " << path << "\n";
    }
#else
#endif
}

static void EnsurePrimaryContextCurrent()
{
    Window* w = Window::Primary();
    if (!w || !w->getSDLWindow() || !w->getGLContext()) {
        return;
    }

    SDL_GLContext current = SDL_GL_GetCurrentContext();
    if (current == w->getGLContext()) {
        return;
    }

    if (!SDL_GL_MakeCurrent(w->getSDLWindow(), w->getGLContext())) {
        std::cerr << "SDL_GL_MakeCurrent failed: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
    }
}

bool Window::SetSwapInterval(int interval)
{
    EnsurePrimaryContextCurrent();

    static int callCount = 0;
    std::cerr << "[SwapInterval] call " << (++callCount)
        << " interval=" << interval << "\n";

    if (!SDL_GL_SetSwapInterval(interval)) {
        std::cerr << "SDL_GL_SetSwapInterval(" << interval << ") failed: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
        return false;
    }

    int actual = 0;
    if (SDL_GL_GetSwapInterval(&actual)) {
        if (actual != interval) {
            std::cerr << "SDL_GL_SetSwapInterval requested " << interval
                << " but actual interval is " << actual
                << " (driver/OS override or unsupported mode)\n";
        }
    }

    return true;
}

bool Window::GetSwapInterval(int& outInterval)
{
    EnsurePrimaryContextCurrent();

    if (!SDL_GL_GetSwapInterval(&outInterval)) {
        std::cerr << "SDL_GL_GetSwapInterval failed: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
        return false;
    }
    return true;
}

bool Window::setVSync(bool enabled)
{
    if (!window || !glContext) {
        std::cerr << "Window::setVSync called without a valid window/context\n";
        return false;
    }

    if (!SDL_GL_MakeCurrent(window, glContext)) {
        std::cerr << "SDL_GL_MakeCurrent failed in setVSync: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
        return false;
    }

    return SetSwapInterval(enabled ? 1 : 0);
}

bool Window::getVSync(bool& outEnabled) const
{
    if (!window || !glContext) {
        std::cerr << "Window::getVSync called without a valid window/context\n";
        return false;
    }

    if (!SDL_GL_MakeCurrent(window, glContext)) {
        std::cerr << "SDL_GL_MakeCurrent failed in getVSync: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
        return false;
    }

    int interval = 0;
    if (!GetSwapInterval(interval)) {
        return false;
    }
    outEnabled = (interval != 0);
    return true;
}

struct GLAttempt {
    int major = 3;
    int minor = 3;
    int profileMask = SDL_GL_CONTEXT_PROFILE_CORE;
    const char* label = "3.3 core";
};

Window::Window(const std::string& title, int width, int height)
    : window(nullptr),
    glContext(nullptr),
    windowTitle(title),
    windowWidth(width),
    windowHeight(height),
    initialized(false)
{
    PrintLoadedSDL3Path();

    if (!SDL_Init(0)) {
        std::cerr << "SDL_Init(0) failed: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << std::endl;
        return;
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: "
            << (SDL_GetError() ? SDL_GetError() : "(no message)") << std::endl;
        SDL_Quit();
        return;
    }

    const GLAttempt tries[] = {
        {4, 6, SDL_GL_CONTEXT_PROFILE_CORE,          "4.6 core"},
        {4, 5, SDL_GL_CONTEXT_PROFILE_CORE,          "4.5 core"},
        {4, 3, SDL_GL_CONTEXT_PROFILE_CORE,          "4.3 core (minimum for compute)"},
        {3, 3, SDL_GL_CONTEXT_PROFILE_CORE,          "3.3 core (fallback)"},
    };

    auto try_one = [&](const GLAttempt& a) -> bool {
        SDL_GL_ResetAttributes();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, a.major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, a.minor);
        if (a.profileMask != 0) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, a.profileMask);
        }
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        window = SDL_CreateWindow(windowTitle.c_str(),
            windowWidth,
            windowHeight,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window) {
            std::cerr << "[GL Attempt " << a.label << "] SDL_CreateWindow failed: "
                << (SDL_GetError() ? SDL_GetError() : "(no message)") << std::endl;
            return false;
        }

        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            std::cerr << "[GL Attempt " << a.label << "] SDL_GL_CreateContext failed: "
                << (SDL_GetError() ? SDL_GetError() : "(no message)") << std::endl;
            SDL_DestroyWindow(window);
            window = nullptr;
            return false;
        }

        SDL_GLContext current = SDL_GL_GetCurrentContext();
        if (current != glContext) {
            if (!SDL_GL_MakeCurrent(window, glContext)) {
                std::cerr << "[GL Attempt " << a.label << "] SDL_GL_MakeCurrent failed: "
                    << (SDL_GetError() ? SDL_GetError() : "(no message)") << std::endl;
                SDL_GL_DestroyContext(glContext);
                glContext = nullptr;
                SDL_DestroyWindow(window);
                window = nullptr;
                return false;
            }
        }

        std::cerr << "[GL Attempt " << a.label << "] success\n";
        return true;
        };

    bool created = false;
    for (const auto& attempt : tries) {
        if (try_one(attempt)) { created = true; break; }
    }

    if (!created) {
        std::cerr << "All GL context attempts failed. Check GPU drivers and ensure SDL3.dll matches your SDL3.lib.\n";
        SDL_Quit();
        return;
    }

    if (!s_primaryWindow) {
        s_primaryWindow = this;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "gladLoadGLLoader failed: SDL_GL_GetProcAddress did not provide core functions.\n";
        assert(glad_glQueryCounter && "glQueryCounter not loaded");
        assert(glad_glGetQueryObjectui64v && "glGetQueryObjectui64v not loaded");

        SDL_GL_DestroyContext(glContext);
        glContext = nullptr;
        SDL_DestroyWindow(window);
        window = nullptr;
        if (s_primaryWindow == this) s_primaryWindow = nullptr;
        SDL_Quit();
        return;
    }

    InstallGLDebugOutput_();

    (void)setVSync(true);
    initialized = true;
}

Window::~Window()
{
    if (s_primaryWindow == this) {
        s_primaryWindow = nullptr;
    }

    if (glContext) {
        SDL_GL_DestroyContext(glContext);
        glContext = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

int Window::getWidth() const { return windowWidth; }
int Window::getHeight() const { return windowHeight; }
std::string Window::getTitle() const { return windowTitle; }
SDL_Window* Window::getSDLWindow() const { return window; }

void Window::setTitle(const std::string& title)
{
    windowTitle = title;
    if (window) SDL_SetWindowTitle(window, title.c_str());
}

void Window::setSize(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    if (window) SDL_SetWindowSize(window, width, height);
}

bool Window::isInitialized() const { return initialized; }

void Window::clear()
{
}

void Window::update()
{
    if (window) SDL_GL_SwapWindow(window);
}

void Window::close()
{
    if (s_primaryWindow == this) {
        s_primaryWindow = nullptr;
    }

    if (glContext) {
        SDL_GL_DestroyContext(glContext);
        glContext = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    initialized = false;
}
