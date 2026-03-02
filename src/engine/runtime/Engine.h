#pragma once

#include "Window.h"
#include "AssetManager.h"
#include "ECS.h"
#include "InputState.h"
#include "InputBackend.h"

#include <cstdint>
#include <memory>

class SystemManager;
class RenderDeviceGL;

class Engine {
public:
    explicit Engine(Window* window);
    ~Engine();

    AssetManager& getAssetManager() { return mAssets; }
    ECS& getECS() { return mECS; }
    SystemManager& getSystemManager();

    bool PollEvents();
    void Update(float dt);
    void Shutdown();

    plat::InputState& getInputState() { return mInputState; }
    const plat::InputState& getInputState() const { return mInputState; }

    bool isGraphicsInitialized() const noexcept { return mGraphicsInitialized; }

private:
    Window* mWindow;
    AssetManager mAssets;
    ECS          mECS;

    std::uint64_t mFrameIndex = 0;
    std::uint64_t mLastPerfCounter = 0;
    std::uint64_t mPerfFreq = 0;

    plat::InputState   mInputState{};
    plat::InputBackend mInputBackend;

    std::unique_ptr<RenderDeviceGL> mRenderDevice;
    bool mGraphicsInitialized = false;
};
