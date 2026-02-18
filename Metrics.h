#pragma once
#include "Stats.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace diag {

    struct ScopeSample {
        const char* name = nullptr;
        double ms = 0.0;
        int calls = 0;
    };

    struct FrameMetrics {
        double cpu_ms = 0.0;
        double gpu_ms = 0.0;
        double fps = 0.0;
        bool spike = false;
    };

    struct EngineMemory {
        uint64_t textures = 0, buffers = 0, meshes = 0, other = 0;
    };

    struct ProcessMemory {
        uint64_t rss_bytes = 0, peak_bytes = 0;
    };

    class MetricsRegistry {
    public:
        explicit MetricsRegistry(int rollingFrames = 600);

        void beginFrame(uint64_t frameIdx);
        void endFrame(uint64_t frameIdx);
        void addCpuScope(const char* name, double ms);
        void addGpuScope(const char* name, double ms);
        void publishProcessMemory(const ProcessMemory& pm) { procMem_ = pm; }
        void publishEngineMemory(const EngineMemory& em) { engMem_ = em; }
        void setCpuFrameMs(double ms) { current_.cpu_ms = ms; }
        void setGpuFrameMs(double ms) { current_.gpu_ms = ms; }
        void setFps(double fps) { current_.fps = fps; }

        const FrameMetrics& currentFrame() const { return current_; }
        const RollingWindow<double>& frameTimesMs() const { return frameTimesMs_; }
        const Percentiles& framePercentiles() const { return lastPct_; }
        const std::vector<ScopeSample>& lastCpuScopes() const { return lastCpuScopes_; }
        const std::vector<ScopeSample>& lastGpuScopes() const { return lastGpuScopes_; }
        const EngineMemory& engineMemory() const { return engMem_; }
        const ProcessMemory& processMemory() const { return procMem_; }

    private:
        uint64_t frameIdx_ = 0;
        RollingWindow<double> frameTimesMs_;
        Percentiles lastPct_{};

        std::unordered_map<std::string, ScopeSample> cpuAccum_;
        std::unordered_map<std::string, ScopeSample> gpuAccum_;

        std::vector<ScopeSample> lastCpuScopes_;
        std::vector<ScopeSample> lastGpuScopes_;

        FrameMetrics current_{};
        EngineMemory engMem_{};
        ProcessMemory procMem_{};
    };

}
