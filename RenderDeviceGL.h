#pragma once

#include <string>

class Window;

struct RenderDeviceGLInfo
{
    bool vsync = true;
};

class RenderDeviceGL
{
public:
    struct Capabilities
    {
        int major = 0;
        int minor = 0;
        std::string vendor;
        std::string renderer;
        std::string version;
    };

    RenderDeviceGL() = default;
    ~RenderDeviceGL() = default;

    RenderDeviceGL(const RenderDeviceGL&) = delete;
    RenderDeviceGL& operator=(const RenderDeviceGL&) = delete;

    bool initialize(Window& window, const RenderDeviceGLInfo& info = {});
    void shutdown();

    void setVsync(bool enabled);

    bool isInitialized() const noexcept { return mInitialized; }
    const Capabilities& caps() const noexcept { return mCaps; }

private:
    Window* mWindow = nullptr;
    bool mInitialized = false;
    bool mVsync = true;
    Capabilities mCaps{};
};
