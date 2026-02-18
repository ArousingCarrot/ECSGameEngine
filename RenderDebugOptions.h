#pragma once

#include <cstdint>

namespace diag {

    enum class DebugView : std::uint32_t
    {
        Lit = 0,
        Albedo,
        Normal,
        UV0,
        Depth
    };

    struct RenderDebugOptions
    {
        bool wireframe = false;
        bool shaderEnabled = true;
        bool materialsEnabled = true;
        bool texturesEnabled = true;
        bool disableCulling = false;
        bool glDebugChecks = false;

        DebugView view = DebugView::Lit;
    };

    RenderDebugOptions& GetRenderDebugOptions();

}
