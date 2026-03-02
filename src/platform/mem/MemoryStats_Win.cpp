#ifdef _WIN32
#include "MemoryStats.h"
#include "GraphicsGL.h"
#include <psapi.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

namespace diag {
    bool GetProcessMemory(ProcessMemory& out) {
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
            return false;
        out.rss_bytes = pmc.WorkingSetSize;
        out.peak_bytes = pmc.PeakWorkingSetSize;
        return true;
    }
}
#endif
