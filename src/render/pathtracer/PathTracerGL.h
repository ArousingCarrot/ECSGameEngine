#pragma once

#include <cstddef>
#include <cstdint>

namespace pt
{
    enum class Denoiser : int
    {
        None = 0,
        AtrousGL = 1,
    };

    enum class DebugView : int
    {
        Denoised = 0,
        Accumulated = 1,
        Sample = 2,
        Albedo = 3,
        Normal = 4,
        Depth = 5,
        RoughMetal = 6,
    };

    struct Settings
    {
        bool enabled = false;
        bool resetAccumulation = false;

        int sppPerFrame = 1;
        float renderScale = 1.0f;
        float exposureEV = 0.0f;

        Denoiser denoiser = Denoiser::AtrousGL;
        DebugView view = DebugView::Denoised;
    };

    struct Stats
    {
        int internalW = 0;
        int internalH = 0;

        std::uint64_t sppAccumulated = 0;

        float msPathTrace = 0.0f;
        float msAccumulate = 0.0f;
        float msDenoise = 0.0f;
        float msTonemap = 0.0f;

        bool usingMeshScene = false;
        std::uint32_t triCount = 0;
        std::uint32_t nodeCount = 0;
        std::uint32_t materialCount = 0;
    };

    struct TriInput
    {
        float v0[3], v1[3], v2[3];
        float n0[3], n1[3], n2[3];

        float uv0[2] = { 0.0f, 0.0f };
        float uv1[2] = { 0.0f, 0.0f };
        float uv2[2] = { 0.0f, 0.0f };

        std::uint32_t material = 0;
    };

    struct MaterialInput
    {
        float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float emissive[3] = { 0.0f, 0.0f, 0.0f };
        float roughness = 0.5f;
        float metallic = 0.0f;

        std::uint32_t baseColorTexGL = 0;
    };

    Settings& GetSettings();
    const Stats& GetStats();

    std::uint32_t GetOutputTextureGL();

    bool Initialize();
    void Shutdown();
    void RequestReset();
    void SetCameraBasis(const float pos[3],
        const float dir[3],
        const float right[3],
        const float up[3],
        float tanHalfFovY);

    void ClearScene();
    bool HasScene();
    void UploadScene(const TriInput* tris, std::size_t triCount,
        const MaterialInput* mats, std::size_t matCount);

    void Render(int viewportW, int viewportH);

    void DrawImGuiPanel();
}
