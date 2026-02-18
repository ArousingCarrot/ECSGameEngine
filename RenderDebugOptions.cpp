#include "RenderDebugOptions.h"

namespace diag {

    RenderDebugOptions& GetRenderDebugOptions() {
        static RenderDebugOptions opts;
        return opts;
    }

}