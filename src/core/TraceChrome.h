#pragma once
#include <string>
#include "Trace.h"

namespace diag {
    bool WriteChromeTraceJSON(const TraceCollector& tc, const std::string& path);
}
