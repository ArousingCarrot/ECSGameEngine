#pragma once
#include "DiagConfig.h"
#include "Metrics.h"
#include "Trace.h"
#include "Overlay.h"

namespace diag {

    class Diagnostics {
    public:
        static Diagnostics& I();

        void initialize(const DiagnosticsConfig& cfg);
        void shutdown();
        void beginFrame(uint64_t frameIdx);
        void endFrame(uint64_t frameIdx, double cpuFrameMs, double gpuFrameMs, double fps);
        void setMode(ProfilerMode m) { mode_ = m; }
        ProfilerMode mode() const { return mode_; }

        void setOverlayVisible(bool v) { overlayVisible_ = v; }
        bool overlayVisible() const { return overlayVisible_; }
        void toggleOverlay() { overlayVisible_ = !overlayVisible_; }

        void drawOverlay();
        void publishEngineMemory(const EngineMemory& em) { metrics_.publishEngineMemory(em); }
        bool saveChromeTrace(const char* path);
        void addCpuScope(const char* name, double ms) { metrics_.addCpuScope(name, ms); }
        void addGpuScope(const char* name, double ms) { metrics_.addGpuScope(name, ms); }

        MetricsRegistry& metrics() { return metrics_; }
        TraceCollector& traces() { return traces_; }

    private:
        DiagnosticsConfig cfg_{};
        MetricsRegistry metrics_{ 600 };
        TraceCollector traces_{};
        Overlay overlay_{};
        ProfilerMode mode_ = ProfilerMode::RollingMinimal;
        bool overlayVisible_ = false;
    };

}
