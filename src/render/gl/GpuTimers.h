#pragma once
#include <cstdint>
#include <vector>

namespace diag {

    using GLQueryID = unsigned int;

    struct GpuScopeResult {
        const char* name;
        uint64_t start_ns;
        uint64_t end_ns;
    };

    class GpuTimerQueryPool {
    public:
        bool initialize();
        void beginFrame();
        void endFrame();

        void beginScope(const char* name);
        void endScope();

        const std::vector<GpuScopeResult>& results() const { return frameResults_; }

    private:
        struct ScopeEntry {
            const char* name;
            GLQueryID start{};
            GLQueryID end{};
        };
        std::vector<ScopeEntry> activeStack_;
        std::vector<ScopeEntry> prevFrameScopes_;
        std::vector<GpuScopeResult> frameResults_;
        std::vector<GLQueryID> freeIds_;
        bool supported_ = false;

        GLQueryID allocQuery();
        void freeQuery(GLQueryID q);
    };

    class ScopedGpuZone {
    public:
        explicit ScopedGpuZone(const char* name);
        ~ScopedGpuZone();
    private:
        const char* name_;
    };
    bool GpuTimingSupported();
    bool BindGlobalGpuPool();
    GpuTimerQueryPool& GetGlobalGpuPool();

}
