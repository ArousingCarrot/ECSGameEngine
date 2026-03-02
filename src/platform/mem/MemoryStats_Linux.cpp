#ifdef __linux__
#include "MemoryStats.h"
#include <cstdio>
#include <cstring>

namespace diag {
    bool GetProcessMemory(ProcessMemory& out) {
        FILE* f = std::fopen("/proc/self/status", "r");
        if (!f) return false;
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "VmRSS:", 6) == 0) {
                unsigned long kb = 0;
                std::sscanf(line + 6, "%lu", &kb);
                out.rss_bytes = kb * 1024UL;
            }
            else if (std::strncmp(line, "VmHWM:", 6) == 0) {
                unsigned long kb = 0;
                std::sscanf(line + 6, "%lu", &kb);
                out.peak_bytes = kb * 1024UL;
            }
        }
        std::fclose(f);
        return true;
    }
}
#endif
