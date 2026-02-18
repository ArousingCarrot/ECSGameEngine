#include "Metrics.h"
#include <algorithm>

namespace diag {

    MetricsRegistry::MetricsRegistry(int rollingFrames)
        : frameTimesMs_(rollingFrames) {
    }

    void MetricsRegistry::beginFrame(uint64_t frameIdx) {
        frameIdx_ = frameIdx;
        cpuAccum_.clear();
        gpuAccum_.clear();
    }

    void MetricsRegistry::endFrame(uint64_t) {
        frameTimesMs_.push(current_.cpu_ms); // CPU frametime as proxy
        auto snap = frameTimesMs_.snapshot();
        lastPct_ = compute_percentiles(snap);
        current_.spike = is_tukey_outlier(current_.cpu_ms, lastPct_);

        // Collapse CPU scope map into a vector sorted by time descending.
        lastCpuScopes_.clear();
        lastCpuScopes_.reserve(cpuAccum_.size());
        for (auto& kv : cpuAccum_) {
            lastCpuScopes_.push_back(kv.second);
        }
        std::sort(
            lastCpuScopes_.begin(),
            lastCpuScopes_.end(),
            [](const ScopeSample& a, const ScopeSample& b) { return a.ms > b.ms; }
        );

        // Collapse GPU scope map into a vector sorted by time descending.
        lastGpuScopes_.clear();
        lastGpuScopes_.reserve(gpuAccum_.size());
        for (auto& kv : gpuAccum_) {
            lastGpuScopes_.push_back(kv.second);
        }
        std::sort(
            lastGpuScopes_.begin(),
            lastGpuScopes_.end(),
            [](const ScopeSample& a, const ScopeSample& b) { return a.ms > b.ms; }
        );
    }

    void MetricsRegistry::addCpuScope(const char* name, double ms) {
        auto& s = cpuAccum_[name ? name : "CPU"];
        s.name = name;
        s.ms += ms;
        s.calls += 1;
    }

    void MetricsRegistry::addGpuScope(const char* name, double ms) {
        auto& s = gpuAccum_[name ? name : "GPU"];
        s.name = name;
        s.ms += ms;
        s.calls += 1;
    }

}
