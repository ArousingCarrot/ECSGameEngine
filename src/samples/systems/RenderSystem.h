#pragma once

#include "ISystem.h"
#include <memory>

class Window;
class AssetManager;
class Shader;
struct MeshAsset;

class RenderSystem : public ISystem {
public:
    RenderSystem(Window* window, AssetManager* assets);
    ~RenderSystem();
    void Update(float dt) override;

private:
    bool ensureSceneTarget(int w, int h);
    void destroySceneTarget();
    void lazyInit();
    void draw(float dt);

    GLuint mSceneFBO = 0;
    GLuint mSceneColorTex = 0;
    GLuint mSceneDepthRBO = 0;
    int mSceneW = 0;
    int mSceneH = 0;

    void drawModelWithMaterials();

    Window* mWindow = nullptr;
    AssetManager* mAssets = nullptr;
    bool initialized = false;

    std::shared_ptr<Shader> shader{};
    std::shared_ptr<MeshAsset> model{};

    GLuint mNullTex = 0;
};
