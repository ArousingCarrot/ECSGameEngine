#pragma once

#include <cstdint>

namespace editor
{
    enum class CenterPane {Scene = 0, Code = 1};

    struct SceneViewportInfo
    {
        int  pixelW = 0;
        int  pixelH = 0;
        bool hovered = false;
        bool focused = false;
        bool clicked = false;
    };
    void SetSceneBounds(const float center[3], float radius);
    bool GetSceneBounds(float outCenter[3], float& outRadius);
    bool ConsumeSceneBoundsUpdate(float outCenter[3], float& outRadius);
    void RequestFrame();
    bool ConsumeFrameRequest();
    void DrawEditorUI();
    void SetSceneTexture(std::uint64_t textureId);
    void SetSceneTexture(std::uint64_t textureId, bool flipY);

    const SceneViewportInfo& GetSceneViewportInfo();
    bool ConsumeSceneClick();
    bool IsCommandPaletteOpen();
    bool WantsTextInput();
    CenterPane GetCenterPane();
}
