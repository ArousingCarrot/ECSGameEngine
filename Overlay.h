#pragma once
#include "Metrics.h"
#include "Trace.h"

namespace diag {

    class Overlay {
    public:
        void draw(const MetricsRegistry& mr, const TraceCollector& tc);
    };

}
