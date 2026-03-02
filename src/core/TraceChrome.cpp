#include "TraceChrome.h"

#include <fstream>
#include <string>

namespace diag {

    static const char* phase(EventType t) {
        switch (t) {
        case EventType::Begin: return "B";
        case EventType::End: return "E";
        case EventType::Instant: return "i";
        }
        return "i";
    }

    bool WriteChromeTraceJSON(const TraceCollector& tc, const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out << "{ \"traceEvents\":[\n";
        const auto& evs = tc.events();
        for (size_t i = 0; i < evs.size(); ++i) {
            const auto& e = evs[i];
            out << " {\"name\":\"" << (e.name ? e.name : "?")
                << "\",\"ph\":\"" << phase(e.type)
                << "\",\"ts\":" << (e.ts_ns / 1000)
                << ",\"pid\":1"
                << ",\"tid\":" << e.tid
                << "}";
            if (i + 1 < evs.size()) out << ",\n";
        }
        out << "\n] }\n";
        return true;
    }

}
