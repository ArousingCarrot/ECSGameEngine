#include "Diagnostics.h"
#include "MemoryStats.h"
#include "TraceChrome.h"
#include "Chrono.h"
#include "GpuTimers.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace diag {

    Diagnostics& Diagnostics::I() {
        static Diagnostics g;
        return g;
    }

    void Diagnostics::initialize(const DiagnosticsConfig& cfg) {
        cfg_ = cfg;
        mode_ = cfg.mode;
        metrics_ = MetricsRegistry(cfg.rollingFrames);

        BindGlobalGpuPool();
    }

    void Diagnostics::shutdown() {
    }

    void Diagnostics::beginFrame(uint64_t frameIdx) {
        if (mode_ == ProfilerMode::Off) return;
        traces_.beginFrame(frameIdx);
        metrics_.beginFrame(frameIdx);
        GetGlobalGpuPool().beginFrame();
    }

    void Diagnostics::endFrame(uint64_t frameIdx, double cpuFrameMs, double gpuFrameMs, double fps) {
        if (mode_ == ProfilerMode::Off) return;

        GetGlobalGpuPool().endFrame();
        const auto& r = GetGlobalGpuPool().results();

        for (auto& s : r) {
            const double ms = (double(s.end_ns - s.start_ns)) / 1'000'000.0;
            metrics_.addGpuScope(s.name, ms);
        }

        if (gpuFrameMs < 0.0) {
            double derived = std::numeric_limits<double>::quiet_NaN();
            for (auto& s : r) {
                if (s.name && std::strcmp(s.name, "Frame") == 0) {
                    derived = (double(s.end_ns - s.start_ns)) / 1'000'000.0;
                    break;
                }
            }
            if (derived == derived) {
                gpuFrameMs = derived;
            }
            else {
                gpuFrameMs = 0.0;
            }
        }

        // Publish CPU/GPU frame times and FPS
        metrics_.setCpuFrameMs(cpuFrameMs);
        metrics_.setGpuFrameMs(gpuFrameMs);
        metrics_.setFps(fps);

        // Process memory
        ProcessMemory pm{};
        if (GetProcessMemory(pm)) metrics_.publishProcessMemory(pm);

        // Aggregate trace (moves thread-local events into collector)
        traces_.endFrame(frameIdx);

        // Finalize metrics (rolling windows, percentiles, spike detection)
        metrics_.endFrame(frameIdx);
    }

    void Diagnostics::drawOverlay() {
        if (!overlayVisible_) return;
        overlay_.draw(metrics_, traces_);
    }

    bool Diagnostics::saveChromeTrace(const char* path) {
        return WriteChromeTraceJSON(traces_, path ? path : "trace.json");
    }

}
